// WindowController members that are plain Qt on every platform. The
// compositor-specific rest lives in WindowController_kwin.cpp / _win.cpp.
#include "WindowController.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRegion>
#include <cstdio>

void WindowController::initFocusTracking() {
    appActive_ = QGuiApplication::focusWindow() != nullptr;
    QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, this,
                     [this](QWindow* w) {
        const bool a = w != nullptr;
        if (a == appActive_) return;
        appActive_ = a;
        emit appActiveChanged();
    });
}

void WindowController::setInputEnabled(QQuickWindow* win, bool enabled) {
    if (!win) return;
    // Input back is the window's SHAPE (WindowShapeItem keeps it on the
    // window), not the whole rectangle, which a null QRegion is; a region
    // fully off the surface makes every pixel click-through.
    win->setProperty("meloInputOff", !enabled);
    win->setMask(enabled ? win->property("meloShape").value<QRegion>() : QRegion(-100, -100, 1, 1));
}

void WindowController::setInputRegion(QQuickWindow* win, int x, int y, int w, int h) {
    if (!win) return;
    // a region of the caller's own, which a shape change must not replace
    win->setProperty("meloInputOff", true);
    win->setMask(QRegion(x, y, w, h));
}

void WindowController::logLine(const QString& text) {
    std::fprintf(stderr, "%s\n", text.toUtf8().constData());
    std::fflush(stderr);
}

void WindowController::copyToClipboard(const QString& text) {
    if (auto* cb = QGuiApplication::clipboard()) cb->setText(text);
}
