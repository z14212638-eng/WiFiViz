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
    if (!window->isWindow())
        return;

    
    window->adjustSize();  
    window->updateGeometry();

    QTimer::singleShot(0, window, [window]() {
        if (!window->isWindow())
            return;
        if (QWidget *parent = window->parentWidget())
        {
            QRect parentRect = parent->window()->frameGeometry();
            QRect winGeom = window->frameGeometry();
            window->move(parentRect.center() - QPoint(winGeom.width() / 2, winGeom.height() / 2));
            return;
        }

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
