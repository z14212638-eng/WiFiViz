#pragma once
#include <QWidget>
#include "utils.h"

class LegendOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit LegendOverlay(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};
