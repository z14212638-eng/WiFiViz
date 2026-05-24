#include "configgraphicsview.h"
#include <QWheelEvent>
#include <QMouseEvent>

ConfigGraphicsView::ConfigGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);

    setDragMode(QGraphicsView::NoDrag);
}

void ConfigGraphicsView::wheelEvent(QWheelEvent *event)
{
    constexpr qreal step = 1.15;
    if (event->angleDelta().y() > 0 && m_scaleFactor < m_maxScale) {
        scale(step, step);
        m_scaleFactor *= step;
    } else if (event->angleDelta().y() < 0 && m_scaleFactor > m_minScale) {
        scale(1.0 / step, 1.0 / step);
        m_scaleFactor /= step;
    }
}

void ConfigGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        setDragMode(ScrollHandDrag);
        QMouseEvent fake(event->type(), event->position(),
                         Qt::LeftButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&fake);
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ConfigGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        setDragMode(NoDrag);
        QMouseEvent fake(event->type(), event->position(),
                         Qt::LeftButton, Qt::NoButton, event->modifiers());
        QGraphicsView::mouseReleaseEvent(&fake);
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
