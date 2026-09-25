#include "SidecarProcess.h"
#include "NodeBootstrap.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <cstdio>
#include "../core/LogClock.h"


SidecarProcess::SidecarProcess(QObject* parent) : QObject(parent) {
    proc_.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&proc_, &QProcess::readyReadStandardOutput, this, &SidecarProcess::onReadyRead);
    connect(&proc_, &QProcess::readyReadStandardError, this, [this] {
        for (const QByteArray& l : proc_.readAllStandardError().split('\n'))
            if (!l.trimmed().isEmpty()) std::fprintf(stderr, "[%9.1f] [sidecar] %s\n", meloLogMs(), l.constData());
    });
    connect(&proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SidecarProcess::onFinished);
}

SidecarProcess::~SidecarProcess() {
    // Without this, ~QProcess kills a live child mid-destruction and the
    // finished() it emits runs onFinished's crash-restart against
    // already-destroyed sibling members: exit heap corruption (intermittent
    // SIGABRT in whatever frees next: QFontCache, thread pools, ...).
    disconnect(&proc_, nullptr, this, nullptr);
    if (proc_.state() != QProcess::NotRunning) {
        // Closing stdin is the shutdown the sidecar honours; SIGKILL skips its exit
        // handler and strands its plugin-host children under init (hosts have no stdin
        // close handler, and Discord's socket keeps its loop alive). Kill only after the
        // timeout.
        proc_.closeWriteChannel();
        if (!proc_.waitForFinished(3000)) {
            std::fprintf(stderr, "[melo] sidecar did not exit on stdin close; killing\n");
            proc_.kill();
            proc_.waitForFinished(2000);
        }
    }
}

QString SidecarProcess::resolveNode() {
    const QByteArray env = qgetenv("MELO_NODE");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env);
#ifdef Q_OS_WIN
    // packaged: node.exe shipped beside melo.exe (scripts/deploy-win.ps1).
    // Version-checked like Linux: below 22.15 the plugin sandbox's net/tls
    // block silently does not install.
    const QString bundled = QCoreApplication::applicationDirPath() + "/node.exe";
    if (QFileInfo::exists(bundled) && nodeVersionOk(bundled)) return bundled;
    const QString sys = QStandardPaths::findExecutable("node");
    if (!sys.isEmpty() && nodeVersionOk(sys)) return sys;
    return {};
#else
    // lite AppImage: system node when it's new enough, else the runtime
    // NodeBootstrap downloaded on a previous run
    const QString sys = QStandardPaths::findExecutable("node");
    if (!sys.isEmpty() && nodeVersionOk(sys)) return sys;
    const QString cached = NodeBootstrap::installedPath();
    if (QFileInfo::exists(cached)) return cached;
    return {};
#endif
}

bool SidecarProcess::nodeVersionOk(const QString& node) {
#ifdef Q_OS_WIN
    // Defender scans a freshly unpacked node.exe on its first run, which can
    // take longer than 3 s; a timeout here fails the first launch for good.
    constexpr int kVersionWaitMs = 10000;
#else
    constexpr int kVersionWaitMs = 3000;
#endif
    QProcess p;
    p.start(node, {"--version"});
    if (!p.waitForFinished(kVersionWaitMs)) { p.kill(); return false; }
    const QString v = QString::fromLatin1(p.readAllStandardOutput()).trimmed();   // "v22.19.0"
    if (!v.startsWith(u'v')) return false;
    const int major = v.mid(1).section(u'.', 0, 0).toInt();
    const int minor = v.mid(1).section(u'.', 1, 1).toInt();
    // 22.15, NOT 22. module.registerHooks landed in 22.15, and it is what
    // withholds net/tls/dgram/http from a plugin without rawNetwork. On
    // 22.0-22.14 that block silently does not install, and a plugin
    // declaring two domains would get raw sockets.
    return major > 22 || (major == 22 && minor >= 15);
}

QString SidecarProcess::resolveBundle() {
    const QByteArray env = qgetenv("MELO_SIDECAR");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env);
#ifdef Q_OS_WIN
    // packaged: sidecar\ beside melo.exe (scripts/deploy-win.ps1)
    const QString beside = QCoreApplication::applicationDirPath() + "/sidecar/melo-sidecar.mjs";
    if (QFileInfo::exists(beside)) return beside;
#endif
    const QString installed = QCoreApplication::applicationDirPath() + "/../share/melo/melo-sidecar.mjs";
    if (QFileInfo::exists(installed)) return installed;
#ifdef MELO_DEV_SIDECAR
    return QStringLiteral(MELO_DEV_SIDECAR);
#else
    return {};
#endif
}

void SidecarProcess::start() {
    const QString node = resolveNode();
    const QString bundle = resolveBundle();
    if (node.isEmpty()) {
#ifdef Q_OS_WIN
        emit permanentlyFailed("No usable Node.js >= 22.15 found (set MELO_NODE)");
#else
        emit nodeMissing();   // main.cpp starts a NodeBootstrap download, then retries start()
#endif
        return;
    }
    if (bundle.isEmpty() || !QFileInfo::exists(bundle)) {
#ifdef Q_OS_WIN
        // A packaged melo has no source tree, so the pnpm hint means nothing there.
        bool devTree = !qEnvironmentVariableIsEmpty("MELO_SIDECAR");
#ifdef MELO_DEV_SIDECAR
        devTree = devTree || QFileInfo::exists(
            QFileInfo(QStringLiteral(MELO_DEV_SIDECAR)).absolutePath() + "/../package.json");
#endif
        if (!devTree) {
            emit permanentlyFailed("melo's sidecar is missing from " +
                                   QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/sidecar") +
                                   ". Reinstall melo.");
            return;
        }
#endif
        emit permanentlyFailed("sidecar bundle not found: " + bundle +
                               " (run: pnpm -C sidecar build)");
        return;
    }
    buf_.clear();
    proc_.start(node, {bundle});
    if (!proc_.waitForStarted(5000)) { emit permanentlyFailed("sidecar failed to start"); return; }
    emit started();
}

void SidecarProcess::stop() {
    disconnect(&proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
               this, &SidecarProcess::onFinished);
    proc_.closeWriteChannel();
    if (!proc_.waitForFinished(2000)) proc_.kill();
}

bool SidecarProcess::running() const {
    return proc_.state() == QProcess::Running;
}

void SidecarProcess::sendLine(const QByteArray& jsonLine) {
    if (running()) proc_.write(jsonLine + '\n');
}

void SidecarProcess::onReadyRead() {
    buf_ += proc_.readAllStandardOutput();
    int nl;
    while ((nl = buf_.indexOf('\n')) >= 0) {
        const QByteArray line = buf_.left(nl);
        buf_.remove(0, nl + 1);
        if (!line.trimmed().isEmpty()) emit lineReceived(line);
    }
}

void SidecarProcess::onFinished(int code, QProcess::ExitStatus status) {
    std::fprintf(stderr, "[sidecar] exited code=%d status=%d\n", code, int(status));
    // BEFORE the restart, because a restarted sidecar has not registered its
    // handlers yet. Without this, ready stays true across a crash and QML
    // issues calls the new process can only answer with `unknown method`.
    emit exited();
    if (!restartWindow_.isValid() || restartWindow_.elapsed() > 60000) {
        restartWindow_.restart();
        restarts_ = 0;
    }
    if (++restarts_ > 3) {
        emit permanentlyFailed("sidecar crashed repeatedly");
        return;
    }
    std::fprintf(stderr, "[sidecar] restarting (%d/3)\n", restarts_);
    start();
}
