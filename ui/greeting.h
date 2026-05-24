#ifndef GREETING_H
#define GREETING_H

#include "utils.h"
#include <QWidget>

namespace Ui {
class Greeting;
}

struct Ns3VersionInfo {
    bool valid = false;
    QString version;   
    QString display;   
};


class Greeting : public QWidget,public ResettableBase
{
    Q_OBJECT

public:
    explicit Greeting(QWidget *parent = nullptr);
    ~Greeting();
    void resetPage() override;
    void setNs3Path(const QString &path);
    QString ns3Path;
 
signals:
    void nextPage();

private slots:
    void on_pushButton_2_clicked();

private:
    Ui::Greeting *ui;
};

#endif // GREETING_H
