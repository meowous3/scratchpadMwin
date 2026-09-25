#pragma once
#include <QDir>
#include <QString>
#include <QUrl>

// A local path as a file URL for QML. Linux keeps the string QML always built
// by hand; Windows needs QUrl's form ("file:///C:/..."), because the hand-built
// "file://C:/..." names a host "C:".
inline QString meloFileUrl(const QString& path) {
#ifdef Q_OS_WIN
    return QUrl::fromLocalFile(QDir::fromNativeSeparators(path)).toString();
#else
    return QStringLiteral("file://") + path;
#endif
}
