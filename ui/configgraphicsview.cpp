#include "configgraphicsview.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QtGlobal>

namespace {

QMouseEvent makeLeftButtonDragEvent(const QMouseEvent *event, Qt::MouseButtons buttons)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QMouseEvent(event->type(), event->position(), event->scenePosition(),
                       event->globalPosition(), Qt::LeftButton, buttons,
                       event->modifiers());
#else
    return QMouseEvent(event->type(), event->localPos(), event->windowPos(),
                       event->screenPos(), Qt::LeftButton, buttons,
                       event->modifiers());
#endif
}

} // namespace

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
        QMouseEvent fake = makeLeftButtonDragEvent(event, Qt::LeftButton);
        QGraphicsView::mousePressEvent(&fake);
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ConfigGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        setDragMode(NoDrag);
        QMouseEvent fake = makeLeftButtonDragEvent(event, Qt::NoButton);
        QGraphicsView::mouseReleaseEvent(&fake);
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
