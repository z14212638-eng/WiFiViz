#include "indus_widget.h"

IndustrialWindow::IndustrialWindow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("IndustrialWindow");

    QFont f = font();
    qreal scale = devicePixelRatioF();
    f.setPointSizeF(12 * scale);
    setFont(f);

    QFile fss(":/qss/dark.qss");
    if (fss.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(fss.readAll());
        fss.close();
    }
}

void IndustrialWindow::centerWindow(QWidget *window)
{
    if (!window)
        return;

    window->adjustSize(); 
    window->updateGeometry();

    QTimer::singleShot(0, window, [window]() {
        QScreen *screen = QGuiApplication::screenAt(window->geometry().center());
        if (!screen)  
            screen = QGuiApplication::primaryScreen();
        QRect screenGeom = screen->geometry();

        QRect winGeom = window->frameGeometry();  
        int x = screenGeom.x() + (screenGeom.width() - winGeom.width()) / 2;
        int y = screenGeom.y() + (screenGeom.height() - winGeom.height()) / 2;

        window->move(x, y);
    });
}
