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
#include <string>
#include <vector>

#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {

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
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &s)) || !s) return {};
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
    dlg->SetFileTypes(UINT(specs.size()), specs.data());
    dlg->SetFileTypeIndex(1);
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
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
        return {};
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    if (multiple) opts |= FOS_ALLOWMULTISELECT;
    dlg->SetOptions(opts);
    const std::wstring wtitle = wide(title);
    if (!title.isEmpty()) dlg->SetTitle(wtitle.c_str());
    std::vector<std::wstring> store;
    applyFilters(dlg.Get(), filterName, patterns, store);
    // Cancel returns HRESULT_FROM_WIN32(ERROR_CANCELLED)
    if (FAILED(dlg->Show(ownerWindow()))) return {};
    ComPtr<IShellItemArray> items;
    if (FAILED(dlg->GetResults(&items))) return {};
    DWORD n = 0;
    items->GetCount(&n);
    QStringList paths;
    for (DWORD i = 0; i < n; ++i) {
        ComPtr<IShellItem> item;
        if (SUCCEEDED(items->GetItemAt(i, &item))) {
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
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg))))
        return {};
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);
    const std::wstring wtitle = wide(title), wname = wide(suggestedName),
                       wext = wide(portalDefaultExtension(patterns));
    if (!title.isEmpty()) dlg->SetTitle(wtitle.c_str());
    if (!suggestedName.isEmpty()) dlg->SetFileName(wname.c_str());
    if (!wext.empty()) dlg->SetDefaultExtension(wext.c_str());
    std::vector<std::wstring> store;
    applyFilters(dlg.Get(), filterName, patterns, store);
    if (FAILED(dlg->Show(ownerWindow()))) return {};
    ComPtr<IShellItem> item;
    if (FAILED(dlg->GetResult(&item))) return {};
    return itemPath(item.Get());
}

}  // namespace

PortalDialog::PortalDialog(QObject* parent) : QObject(parent) {}

// Deferred like the D-Bus backend's asynchronous reply: the QML handler that
// asked returns before the result arrives.
void PortalDialog::openFile(const QString& tag, const QString& title,
                            const QString& filterName, const QStringList& patterns,
                            bool multiple) {
    QTimer::singleShot(0, this, [=] {
        const QStringList paths = runOpen(title, filterName, patterns, multiple);
        if (paths.isEmpty()) emit cancelled(tag);
        else emit picked(tag, paths);
    });
}

void PortalDialog::saveFile(const QString& tag, const QString& title,
                            const QString& suggestedName, const QString& filterName,
                            const QStringList& patterns) {
    QTimer::singleShot(0, this, [=] {
        const QString path = runSave(title, suggestedName, filterName, patterns);
        if (path.isEmpty()) emit cancelled(tag);
        else emit picked(tag, {path});
    });
}
