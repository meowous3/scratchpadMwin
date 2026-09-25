#pragma once
#include <QList>
#include <QString>
#include <QStringList>

// The file-type filter a Portal call asks for, as the Windows Common Item
// Dialog takes it. Separate from PortalDialog_win.cpp so it is tested on
// every platform.
struct PortalFilter {
    QString name;   // "Images"
    QString spec;   // "*.png;*.jpg"
};

inline QList<PortalFilter> portalFilters(const QString& filterName, const QStringList& patterns) {
    if (patterns.isEmpty()) return {};
    const QString spec = patterns.join(QLatin1Char(';'));
    return {{filterName.isEmpty() ? spec : filterName, spec}};
}

// "json" from "*.json": what a save dialog appends when the user types a bare
// name. Empty for "*" or no patterns.
inline QString portalDefaultExtension(const QStringList& patterns) {
    if (patterns.isEmpty()) return {};
    const QString& p = patterns.first();
    if (!p.startsWith(QLatin1String("*.")) || p.size() < 3) return {};
    return p.mid(2);
}
