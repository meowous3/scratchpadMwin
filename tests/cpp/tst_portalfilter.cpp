// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "PortalFilter.h"

// The Windows dialog's file-type list, built from the same (name, patterns)
// every Portal call site passes: one entry, patterns joined the way
// COMDLG_FILTERSPEC wants them. No patterns: no filter, every file shows.
class TstPortalFilter : public QObject {
    Q_OBJECT
private slots:
    void onePatternList() {
        const auto f = portalFilters("Images", {"*.png", "*.jpg", "*.svg"});
        QCOMPARE(f.size(), 1);
        QCOMPARE(f[0].name, QStringLiteral("Images"));
        QCOMPARE(f[0].spec, QStringLiteral("*.png;*.jpg;*.svg"));
    }
    void unnamedFallsBackToSpec() {
        const auto f = portalFilters(QString(), {"*.json"});
        QCOMPARE(f[0].name, QStringLiteral("*.json"));
    }
    void noPatternsNoFilter() { QVERIFY(portalFilters("Audio", {}).isEmpty()); }
    void defaultExtension() {
        QCOMPARE(portalDefaultExtension({"*.json"}), QStringLiteral("json"));
        QCOMPARE(portalDefaultExtension({"*.melo-theme", "*.json"}), QStringLiteral("melo-theme"));
        QCOMPARE(portalDefaultExtension({"*"}), QString());
        QCOMPARE(portalDefaultExtension({}), QString());
    }
};

QTEST_GUILESS_MAIN(TstPortalFilter)
#include "tst_portalfilter.moc"
