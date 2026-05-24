#pragma once
#include <QGraphicsView>
#include "utils.h"

class ConfigGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ConfigGraphicsView(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    qreal m_scaleFactor = 1.0;
    const qreal m_minScale = 0.05;
    const qreal m_maxScale = 10.0;
};
