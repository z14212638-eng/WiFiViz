#ifndef PAGE1_MODEL_CHOSE_H
#define PAGE1_MODEL_CHOSE_H

#include <QWidget>
#include <QEvent>
#include <QMouseEvent>
#include <QTextBrowser>
#include <QScrollBar>
#include <QListWidget>


#include "simu_config.h"
#include "utils.h"
#include "image_viewer.h"

namespace Ui {
class Page1_model_chose;
}

class Page1_model_chose : public QWidget, public ResettableBase
{
    Q_OBJECT

public:
    explicit Page1_model_chose(QWidget *parent = nullptr);
    ~Page1_model_chose();
    QString GetSceneName();
    QString Scene = "";
    QString ns3Path = "";
    Simu_Config *simu_config = new(Simu_Config);
    void resetPage() override;
    void refreshModelLists();
    void setJsonHelper(JsonHelper *helper);
private slots:
    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_clicked();
    
    void on_load_button_clicked();

signals:
    void BackToMain();
    void ConfigSimulation();
    void RunSelectedSimulation();
    void LoadProjectRequested(const QString &projectPath);
private:
    Ui::Page1_model_chose *ui;
    QString sceneName = "";
    bool eventFilter(QObject *obj, QEvent *event) override;
    QListWidget *currentSceneList() const;
    QString currentSceneBaseDir() const;
    void updateMarkdownForSelection();
    JsonHelper *m_jsonHelper = nullptr;
    
};

#endif // PAGE1_MODEL_CHOSE_H
