#ifndef RANDOM_VARIABLE_DIALOG_H
#define RANDOM_VARIABLE_DIALOG_H

#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMap>
#include <QString>

class RandomVariableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RandomVariableDialog(const QString &type, QWidget *parent = nullptr);
    ~RandomVariableDialog();

    // 获取参数
    QMap<QString, double> getParameters() const;
    // 设置参数
    void setParameters(const QMap<QString, double> &params);

private:
    QString m_type;
    QFormLayout *m_formLayout;
    QMap<QString, QDoubleSpinBox*> m_spinBoxes;
    QDialogButtonBox *m_buttonBox;

    void setupUI();
};

#endif // RANDOM_VARIABLE_DIALOG_H
