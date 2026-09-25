#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "paths.h"

// Where melo keeps settings, library and plugins. MELO_CONFIG_DIR always
// wins; Windows then prefers a portable folder, then %APPDATA%; Linux keeps
// ~/.config/melo whatever sits beside the binary.
class TstPaths : public QObject {
    Q_OBJECT
    QByteArray savedAppData_;
    bool hadAppData_ = false;

private slots:
    void initTestCase() {
        hadAppData_ = qEnvironmentVariableIsSet("APPDATA");
        savedAppData_ = qgetenv("APPDATA");
    }
    void init() { qunsetenv("MELO_CONFIG_DIR"); }
    void cleanup() {
        if (hadAppData_) qputenv("APPDATA", savedAppData_);
        else qunsetenv("APPDATA");
    }

    void envOverrideWins() {
        QTemporaryDir exe;
        QVERIFY(exe.isValid());
        QVERIFY(QFile(exe.path() + "/portable").open(QIODevice::WriteOnly));
        qputenv("MELO_CONFIG_DIR", "/somewhere/else");
        QCOMPARE(meloConfigDirFrom(exe.path()), QStringLiteral("/somewhere/else"));
    }

    void portableMarker() {
        QTemporaryDir exe(QDir::tempPath() + "/melo Zoë XXXXXX");   // space + non-ASCII
        QVERIFY(exe.isValid());
        QVERIFY(QFile(exe.path() + "/portable").open(QIODevice::WriteOnly));
#ifdef Q_OS_WIN
        QCOMPARE(meloConfigDirFrom(exe.path()), exe.path() + "/data");
#else
        QCOMPARE(meloConfigDirFrom(exe.path()), QDir::homePath() + "/.config/melo");
#endif
    }

    void installedDefault() {
        QTemporaryDir exe;
        QVERIFY(exe.isValid());
#ifdef Q_OS_WIN
        // toLocal8Bit: qputenv hands bytes to the CRT in the ANSI code page
        qputenv("APPDATA", QStringLiteral("C:\\Users\\Zoë Smith\\AppData\\Roaming").toLocal8Bit());
        QCOMPARE(meloConfigDirFrom(exe.path()),
                 QStringLiteral("C:/Users/Zoë Smith/AppData/Roaming/melo"));
#else
        QCOMPARE(meloConfigDirFrom(exe.path()), QDir::homePath() + "/.config/melo");
#endif
    }
};

QTEST_GUILESS_MAIN(TstPaths)
#include "tst_paths.moc"
