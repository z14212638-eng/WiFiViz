#include "image_viewer.h"
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QKeyEvent>

ImageView::ImageView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void ImageView::wheelEvent(QWheelEvent *event)
{
    const double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0)
        scale(scaleFactor, scaleFactor);
    else
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
}

/* ================= ImageViewer ================= */

// 原有构造：引用
ImageViewer::ImageViewer(const QPixmap &pixmap, QWidget *parent)
    : QDialog(parent)
{
    init(pixmap);
}

// ✅ 新增构造：指针（给 QLabel::pixmap() 用）
ImageViewer::ImageViewer(const QPixmap *pixmap, QWidget *parent)
    : QDialog(parent)
{
    if (!pixmap || pixmap->isNull())
        return;

    init(*pixmap);
}

// 公共初始化逻辑
void ImageViewer::init(const QPixmap &pixmap)
{
    setWindowTitle("Image Viewer");
    resize(900, 600);

    m_view = new ImageView(this);
    m_scene = new QGraphicsScene(this);
    m_item = new QGraphicsPixmapItem(pixmap);

    m_scene->addItem(m_item);
    m_view->setScene(m_scene);
    m_view->fitInView(m_item, Qt::KeepAspectRatio);
    m_view->scale(8.25, 8.25); 

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    setLayout(layout);
}

// ESC 关闭
void ImageViewer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        close();
        return;
    }
    QDialog::keyPressEvent(event);
}
