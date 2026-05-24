#pragma once 
#include <QWidget>
#include <QApplication>
#include <QFile>
#include <QScreen>
#include <QTimer>

class IndustrialWindow : public QWidget
{
    Q_OBJECT  // 必须有

public:
    explicit IndustrialWindow(QWidget *parent = nullptr);
    virtual ~IndustrialWindow() = default;

    void centerWindow(QWidget *window);
};