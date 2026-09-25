// Windows backend for Portal: every request cancels until the Common Item
// Dialog backend lands.
#include "PortalDialog.h"
PortalDialog::PortalDialog(QObject* parent) : QObject(parent) {}
void PortalDialog::openFile(const QString& tag, const QString&, const QString&,
                            const QStringList&, bool) { emit cancelled(tag); }
void PortalDialog::saveFile(const QString& tag, const QString&, const QString&,
                            const QString&, const QStringList&) { emit cancelled(tag); }
