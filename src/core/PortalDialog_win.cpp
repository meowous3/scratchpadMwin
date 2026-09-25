// SPDX-License-Identifier: GPL-3.0-or-later
// Windows backend for Portal: the shell's Common Item Dialogs. Same API and
// signals as the XDG portal backend (PortalDialog.cpp), so QML is unchanged
// and QtQuick.Dialogs stays out of the build (and out of plugin engines).
#include "PortalDialog.h"
#include "PortalFilter.h"

#include <QDir>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {

// A real failure reaches melo.log; the caller still just sees cancelled().
// The user dismissing the dialog (ERROR_CANCELLED) is not a failure.
bool ok(HRESULT hr, const char* step) {
    if (SUCCEEDED(hr)) return true;
    if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
        std::fprintf(stderr, "[portal] file dialog failed at %s (0x%08lx)\n", step,
                     static_cast<unsigned long>(hr));
    return false;
}

// Modal on the focused melo window, else any visible one.
HWND ownerWindow() {
    QWindow* w = QGuiApplication::focusWindow();
    if (!w)
        for (QWindow* t : QGuiApplication::topLevelWindows())
            if (t->isVisible()) { w = t; break; }
    return w ? reinterpret_cast<HWND>(w->winId()) : nullptr;
}

QString itemPath(IShellItem* item) {
    PWSTR s = nullptr;
    if (!ok(item->GetDisplayName(SIGDN_FILESYSPATH, &s), "GetDisplayName") || !s) return {};
    const QString out = QDir::fromNativeSeparators(QString::fromWCharArray(s));
    CoTaskMemFree(s);
    return out;
}

std::wstring wide(const QString& s) { return s.toStdWString(); }

// COMDLG_FILTERSPEC holds pointers, so the strings live in `store` for the
// dialog's lifetime.
void applyFilters(IFileDialog* dlg, const QString& filterName, const QStringList& patterns,
                  std::vector<std::wstring>& store) {
    const QList<PortalFilter> filters = portalFilters(filterName, patterns);
    if (filters.isEmpty()) return;
    store.reserve(filters.size() * 2);
    std::vector<COMDLG_FILTERSPEC> specs;
    for (const PortalFilter& f : filters) {
        store.push_back(wide(f.name));
        store.push_back(wide(f.spec));
        specs.push_back({store[store.size() - 2].c_str(), store.back().c_str()});
    }
    ok(dlg->SetFileTypes(UINT(specs.size()), specs.data()), "SetFileTypes");
    ok(dlg->SetFileTypeIndex(1), "SetFileTypeIndex");
}

// COM on the GUI thread. Qt's Windows plugin has usually initialised OLE
// already (S_FALSE); only a call that succeeded is balanced.
struct ComApartment {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    ~ComApartment() { if (SUCCEEDED(hr)) CoUninitialize(); }
};

QStringList runOpen(const QString& title, const QString& filterName,
                    const QStringList& patterns, bool multiple) {
    ComApartment com;
    ComPtr<IFileOpenDialog> dlg;
    if (!ok(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(&dlg)), "CoCreateInstance(FileOpenDialog)"))
        return {};
    DWORD opts = 0;
    ok(dlg->GetOptions(&opts), "GetOptions");
    opts |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    if (multiple) opts |= FOS_ALLOWMULTISELECT;
    ok(dlg->SetOptions(opts), "SetOptions");
    const std::wstring wtitle = wide(title);
    if (!title.isEmpty()) ok(dlg->SetTitle(wtitle.c_str()), "SetTitle");
    std::vector<std::wstring> store;
    applyFilters(dlg.Get(), filterName, patterns, store);
    // Cancel returns HRESULT_FROM_WIN32(ERROR_CANCELLED); ok() stays quiet on it
    if (!ok(dlg->Show(ownerWindow()), "Show")) return {};
    ComPtr<IShellItemArray> items;
    if (!ok(dlg->GetResults(&items), "GetResults")) return {};
    DWORD n = 0;
    items->GetCount(&n);
    QStringList paths;
    for (DWORD i = 0; i < n; ++i) {
        ComPtr<IShellItem> item;
        if (ok(items->GetItemAt(i, &item), "GetItemAt")) {
            const QString p = itemPath(item.Get());
            if (!p.isEmpty()) paths << p;
        }
    }
    return paths;
}

QString runSave(const QString& title, const QString& suggestedName,
                const QString& filterName, const QStringList& patterns) {
    ComApartment com;
    ComPtr<IFileSaveDialog> dlg;
    if (!ok(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(&dlg)), "CoCreateInstance(FileSaveDialog)"))
        return {};
    DWORD opts = 0;
    ok(dlg->GetOptions(&opts), "GetOptions");
    ok(dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST),
       "SetOptions");
    const std::wstring wtitle = wide(title), wname = wide(suggestedName),
                       wext = wide(portalDefaultExtension(patterns));
    if (!title.isEmpty()) ok(dlg->SetTitle(wtitle.c_str()), "SetTitle");
    if (!suggestedName.isEmpty()) ok(dlg->SetFileName(wname.c_str()), "SetFileName");
    if (!wext.empty()) ok(dlg->SetDefaultExtension(wext.c_str()), "SetDefaultExtension");
    std::vector<std::wstring> store;
    applyFilters(dlg.Get(), filterName, patterns, store);
    if (!ok(dlg->Show(ownerWindow()), "Show")) return {};
    ComPtr<IShellItem> item;
    if (!ok(dlg->GetResult(&item), "GetResult")) return {};
    return itemPath(item.Get());
}

// Show() runs a nested event loop, so a second request can arrive while a
// dialog is up; a second modal on the same owner would stack under it.
bool g_dialogShowing = false;

struct ShowingGuard {
    ShowingGuard() { g_dialogShowing = true; }
    ~ShowingGuard() { g_dialogShowing = false; }
};

bool refuseWhileShowing(const QString& tag) {
    if (!g_dialogShowing) return false;
    std::fprintf(stderr, "[portal] file dialog already open; cancelled %s\n", qPrintable(tag));
    return true;
}

}  // namespace

PortalDialog::PortalDialog(QObject* parent) : QObject(parent) {}

// Deferred like the D-Bus backend's asynchronous reply: the QML handler that
// asked returns before the result arrives.
void PortalDialog::openFile(const QString& tag, const QString& title,
                            const QString& filterName, const QStringList& patterns,
                            bool multiple) {
    QTimer::singleShot(0, this, [=] {
        if (refuseWhileShowing(tag)) { emit cancelled(tag); return; }
        const ShowingGuard showing;
        const QStringList paths = runOpen(title, filterName, patterns, multiple);
        if (paths.isEmpty()) emit cancelled(tag);
        else emit picked(tag, paths);
    });
}

void PortalDialog::saveFile(const QString& tag, const QString& title,
                            const QString& suggestedName, const QString& filterName,
                            const QStringList& patterns) {
    QTimer::singleShot(0, this, [=] {
        if (refuseWhileShowing(tag)) { emit cancelled(tag); return; }
        const ShowingGuard showing;
        const QString path = runSave(title, suggestedName, filterName, patterns);
        if (path.isEmpty()) emit cancelled(tag);
        else emit picked(tag, {path});
    });
}
