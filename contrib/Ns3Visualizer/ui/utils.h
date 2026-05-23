// utils.h
#pragma once
#include <QWidget>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>

inline void centerWindow(QWidget *window)
{
    if (!window)
        return;

    
    window->adjustSize();  
    window->updateGeometry();

    QTimer::singleShot(0, window, [window]() {
        QScreen *screen = QGuiApplication::screenAt(window->geometry().center());
        if (!screen)  // fallback
            screen = QGuiApplication::primaryScreen();
        QRect screenGeom = screen->geometry();

        QRect winGeom = window->frameGeometry(); 
        int x = screenGeom.x() + (screenGeom.width() - winGeom.width()) / 2;
        int y = screenGeom.y() + (screenGeom.height() - winGeom.height()) / 2;

        window->move(x, y);
    });
}

class ResettableBase
{
    public:
    virtual ~ResettableBase() = default;
    virtual void resetPage() = 0;
};