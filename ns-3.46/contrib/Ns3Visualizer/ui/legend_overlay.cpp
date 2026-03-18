#include "legend_overlay.h"
#include <QPainter>

LegendOverlay::LegendOverlay(QWidget *parent)
    : QWidget(parent,
              Qt::Tool |
              Qt::FramelessWindowHint |
              Qt::BypassWindowManagerHint)
{
    setFixedSize(280, 320);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

void LegendOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 80));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    p.setPen(QColor(220, 220, 220)); 
    int y = 24;

    auto drawItem = [&](const QColor &c, const QString &txt)
    {
        p.setBrush(c);
        p.drawRect(12, y - 10, 14, 10);
        p.drawText(36, y, txt);
        y += 22;
    };

    auto drawArrowItem = [&](const QString &txt)
    {
        // Draw horizontal arrow example (indicating data flow)
        p.setPen(QPen(QColor(255, 255, 255, 220), 2));
        p.setBrush(QColor(255, 255, 255, 220));
        // Draw arrow line
        p.drawLine(12, y - 5, 22, y - 5);
        // Draw arrow head (pointing right)
        QPolygonF arrowHead;
        arrowHead << QPointF(22, y - 5)
                  << QPointF(18, y - 8)
                  << QPointF(18, y - 2);
        p.drawPolygon(arrowHead);
        p.drawText(36, y, txt);
        y += 22;
    };

    // Title
    p.setPen(QColor(255, 255, 255));
    p.setFont(QFont("Arial", 10, QFont::Bold));
    p.drawText(12, 18, "Legend");
    p.setFont(QFont("Arial", 9));
    p.setPen(QColor(220, 220, 220));
    y = 36;

    // PPDU colors
    drawItem(QColor("#1f77b4"), "Normal PPDU");
    
    // Collision PPDU (with overlay effect)
    p.setBrush(QColor("#1f77b4"));
    p.drawRect(12, y - 10, 14, 10);
    p.setBrush(QColor(255, 127, 14, 180)); // Semi-transparent orange overlay
    p.drawRect(12, y - 10, 14, 10);
    p.setPen(QColor(220, 220, 220));
    p.setBrush(Qt::NoBrush);
    p.drawRect(12, y - 10, 14, 10);
    p.drawText(36, y, "Collision PPDU");
    y += 22;
    
    // Hovered
    drawItem(Qt::red, "Hovered PPDU");

    // Channel-state colors
    drawItem(QColor(150, 164, 180, 150), "Channel IDLE");
    drawItem(QColor(62, 140, 94, 210), "Channel BUSY");
    drawItem(QColor(220, 88, 64, 220), "Channel COLLISION");

    // PHY-state colors
    drawItem(QColor(150, 164, 180, 150), "PHY IDLE");
    drawItem(QColor(223, 170, 58, 220), "PHY CCA_BUSY");
    drawItem(QColor(62, 140, 94, 220), "PHY TX");
    drawItem(QColor(76, 114, 176, 220), "PHY RX");
    
    // Data flow
    drawArrowItem("Data Flow");
}
