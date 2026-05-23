#include "random_variable_dialog.h"
#include "config_ui_style.h"
#include <QVBoxLayout>

RandomVariableDialog::RandomVariableDialog(const QString &type, QWidget *parent)
    : QDialog(parent)
    , m_type(type)
{
    setupUI();
    setObjectName("RandomVariableDialog");
    ConfigUiStyle::ApplyConfigTheme(this);
    setWindowTitle(tr("Random Variable Parameters - %1").arg(type));
    setModal(true);
}

RandomVariableDialog::~RandomVariableDialog()
{
}

void RandomVariableDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_formLayout = new QFormLayout();
    
    // 根据不同的随机变量类型添加不同的参数输入框
    if (m_type == "Constant") {
        QDoubleSpinBox *valueBox = new QDoubleSpinBox(this);
        valueBox->setRange(0.0, 1000000.0);
        valueBox->setDecimals(6);
        valueBox->setValue(1.0);
        m_spinBoxes["Value"] = valueBox;
        m_formLayout->addRow(tr("Value:"), valueBox);
    }
    else if (m_type == "Uniform") {
        QDoubleSpinBox *minBox = new QDoubleSpinBox(this);
        minBox->setRange(0.0, 1000000.0);
        minBox->setDecimals(6);
        minBox->setValue(0.0);
        m_spinBoxes["Min"] = minBox;
        m_formLayout->addRow(tr("Min:"), minBox);
        
        QDoubleSpinBox *maxBox = new QDoubleSpinBox(this);
        maxBox->setRange(0.0, 1000000.0);
        maxBox->setDecimals(6);
        maxBox->setValue(1.0);
        m_spinBoxes["Max"] = maxBox;
        m_formLayout->addRow(tr("Max:"), maxBox);
    }
    else if (m_type == "Exponential") {
        QDoubleSpinBox *meanBox = new QDoubleSpinBox(this);
        meanBox->setRange(0.0, 1000000.0);
        meanBox->setDecimals(6);
        meanBox->setValue(1.0);
        m_spinBoxes["Mean"] = meanBox;
        m_formLayout->addRow(tr("Mean:"), meanBox);
        
        QDoubleSpinBox *boundBox = new QDoubleSpinBox(this);
        boundBox->setRange(0.0, 1000000.0);
        boundBox->setDecimals(6);
        boundBox->setValue(0.0);
        boundBox->setSpecialValueText(tr("Infinite"));
        m_spinBoxes["Bound"] = boundBox;
        m_formLayout->addRow(tr("Upper Bound:"), boundBox);
    }
    else if (m_type == "Normal" || m_type == "Gaussian") {
        QDoubleSpinBox *meanBox = new QDoubleSpinBox(this);
        meanBox->setRange(-1000000.0, 1000000.0);
        meanBox->setDecimals(6);
        meanBox->setValue(0.0);
        m_spinBoxes["Mean"] = meanBox;
        m_formLayout->addRow(tr("Mean:"), meanBox);
        
        QDoubleSpinBox *varianceBox = new QDoubleSpinBox(this);
        varianceBox->setRange(0.0, 1000000.0);
        varianceBox->setDecimals(6);
        varianceBox->setValue(1.0);
        m_spinBoxes["Variance"] = varianceBox;
        m_formLayout->addRow(tr("Variance:"), varianceBox);
        
        QDoubleSpinBox *boundBox = new QDoubleSpinBox(this);
        boundBox->setRange(0.0, 1000000.0);
        boundBox->setDecimals(6);
        boundBox->setValue(0.0);
        boundBox->setSpecialValueText(tr("Infinite"));
        m_spinBoxes["Bound"] = boundBox;
        m_formLayout->addRow(tr("Bound:"), boundBox);
    }
    
    mainLayout->addLayout(m_formLayout);
    
    // 添加按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
    
    setLayout(mainLayout);
}

QMap<QString, double> RandomVariableDialog::getParameters() const
{
    QMap<QString, double> params;
    for (auto it = m_spinBoxes.begin(); it != m_spinBoxes.end(); ++it) {
        params[it.key()] = it.value()->value();
    }
    return params;
}

void RandomVariableDialog::setParameters(const QMap<QString, double> &params)
{
    for (auto it = params.begin(); it != params.end(); ++it) {
        if (m_spinBoxes.contains(it.key())) {
            m_spinBoxes[it.key()]->setValue(it.value());
        }
    }
}
