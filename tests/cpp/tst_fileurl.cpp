#include <QtTest>
#include <QUrl>

#include "FileUrl.h"

// A local path as the URL QML hands to Image.source / openUrlExternally.
// Linux keeps the exact string QML built before ("file://" + path); Windows
// needs a real file URL, since "file://C:/x" makes "C:" the host.
class TstFileUrl : public QObject {
    Q_OBJECT
private slots:
    void linuxUnchanged() {
#ifdef Q_OS_WIN
        QSKIP("Linux form");
#else
        QCOMPARE(meloFileUrl("/home/zoë/Music/a b/c#1.png"),
                 QStringLiteral("file:///home/zoë/Music/a b/c#1.png"));
#endif
    }
    void windowsDriveLetter() {
#ifndef Q_OS_WIN
        QSKIP("Windows form");
#else
        const QString u = meloFileUrl("C:/Users/Zoë Smith/Music/a b.png");
        QVERIFY(u.startsWith("file:///C:/"));
        QCOMPARE(QUrl(u).toLocalFile(), QStringLiteral("C:/Users/Zoë Smith/Music/a b.png"));
        QCOMPARE(QUrl(meloFileUrl("C:\\Users\\x\\y.png")).toLocalFile(),
                 QStringLiteral("C:/Users/x/y.png"));
        QCOMPARE(QUrl(meloFileUrl("C:/a/c#1 50%.png")).toLocalFile(),
                 QStringLiteral("C:/a/c#1 50%.png"));
#endif
    }
};

QTEST_GUILESS_MAIN(TstFileUrl)
#include "tst_fileurl.moc"
