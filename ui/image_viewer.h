#pragma once

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

class ImageView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
};

class ImageViewer : public QDialog
{
    Q_OBJECT
public:
    // 原有：引用版本（保留）
    explicit ImageViewer(const QPixmap &pixmap, QWidget *parent = nullptr);

    // ✅ 新增：指针版本（直接兼容 QLabel::pixmap()）
    explicit ImageViewer(const QPixmap *pixmap, QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void init(const QPixmap &pixmap);

private:
    ImageView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_item = nullptr;
};
