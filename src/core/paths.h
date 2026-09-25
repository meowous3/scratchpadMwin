#pragma once
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

// melo's config/data directory: the sidecar's "dataDir" (SidecarService::initialize),
// SettingsStore::dataDir(), and the root of plugins/ and plugin-data/. Do not
// re-derive the path. MELO_CONFIG_DIR lets a test instance run beside a live melo
// without racing its settings, library and cookie files.
//
// exeDir is melo.exe's folder, passed in so main() can resolve the directory
// before QCoreApplication exists (the Windows log lives there). Windows: an
// empty `portable` file beside melo.exe keeps everything in <exeDir>/data.
inline QString meloConfigDirFrom(const QString& exeDir) {
    const QByteArray env = qgetenv("MELO_CONFIG_DIR");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env);
#ifdef Q_OS_WIN
    if (!exeDir.isEmpty() && QFileInfo::exists(exeDir + QStringLiteral("/portable")))
        return exeDir + QStringLiteral("/data");
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) return QDir::fromNativeSeparators(appData) + QStringLiteral("/melo");
#else
    Q_UNUSED(exeDir);
#endif
    return QDir::homePath() + "/.config/melo";
}

inline QString meloConfigDir() {
#ifdef Q_OS_WIN
    return meloConfigDirFrom(QCoreApplication::applicationDirPath());
#else
    return meloConfigDirFrom(QString());
#endif
}
