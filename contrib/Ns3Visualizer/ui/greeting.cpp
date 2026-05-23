#include "greeting.h"
#include "ui_greeting.h"
#include "config_ui_style.h"
#include <QFileDialog>
#include <QRegularExpression>
#include <QTextStream>

Greeting::Greeting(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Greeting)
{
    ui->setupUi(this);
    ConfigUiStyle::ApplyConfigTheme(this);
    ui->lineEdit->setReadOnly(true);
    ui->lineEdit->setFocusPolicy(Qt::NoFocus);

#ifdef NS3_SOURCE_DIR
    setNs3Path(QString(NS3_SOURCE_DIR));
#endif

    QLabel *title = ui->title; // objectName = title

    title->setFont(QFont("Inter", 50, QFont::DemiBold));
    title->setStyleSheet(R"(
        color: #C7CBD1;
        letter-spacing: 3px;
        padding-bottom: 8px;
        border-bottom: 2px solid #C7CBD1;
    )");

}

Greeting::~Greeting()
{
    delete ui;
}

void Greeting::resetPage()
{
    if (!ns3Path.isEmpty())
        ui->lineEdit->setText(ns3Path);
    qDebug()<<"GreetingPage resetPage()";
}

void Greeting::on_pushButton_2_clicked()
{
    if(this->ns3Path.isEmpty())
        return;
    emit nextPage();
}

void Greeting::setNs3Path(const QString &path)
{
    ns3Path = path;
    ui->lineEdit->setText(path);
}

Ns3VersionInfo detectNs3Version(const QString& ns3Dir)
{
    Ns3VersionInfo info;

    QFile versionFile(ns3Dir + "/VERSION");
    if (versionFile.exists() && versionFile.open(QIODevice::ReadOnly)) {
        QString v = QString::fromUtf8(versionFile.readAll()).trimmed();

        if (!v.isEmpty()) {
            info.valid = true;
            info.version = v;

            if (v == "dev")
                info.display = "ns-3-dev (Development)";
            else
                info.display = "ns-3." + v + " (Release)";

            return info;
        }
    }

    QFile cmakeFile(ns3Dir + "/CMakeLists.txt");
    if (cmakeFile.exists() && cmakeFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&cmakeFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QRegularExpression re(R"(NS3_VERSION\s+([0-9\.]+|dev))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                QString v = match.captured(1);
                info.valid = true;
                info.version = v;

                if (v == "dev")
                    info.display = "ns-3-dev (Development)";
                else
                    info.display = "ns-3." + v + " (Release)";

                return info;
            }
        }
    }

    return info; // invalid
}
