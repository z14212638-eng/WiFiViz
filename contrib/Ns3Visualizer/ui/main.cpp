#include "mainwindow.h"
#include "timeline_window.h"
#include "visualizer_mode.h"
#include "visualizer_config.h"
#include <QApplication>
#include <QProcess>
#include <QFile>
#include <cstdlib>

namespace
{
void
ApplySamplingConfig(const QStringList& args)
{
    bool precise = true;
    int sampleRate = 1;

    for (int i = 1; i < args.size(); ++i)
    {
        const QString arg = args[i];
        if (arg == "--precise" || arg == "--precise=1" || arg == "--precise=true")
        {
            precise = true;
            continue;
        }
        if (arg == "--rough")
        {
            precise = false;
            sampleRate = 10;
            continue;
        }
        if (arg.startsWith("--rough="))
        {
            bool ok = false;
            const int value = arg.mid(QString("--rough=").size()).toInt(&ok);
            if (ok && value > 1)
            {
                precise = false;
                sampleRate = value;
            }
            continue;
        }
    }

    const bool hasCliPrecise = args.contains("--precise") ||
                               args.contains("--precise=1") ||
                               args.contains("--precise=true");
    bool hasCliRough = false;
    for (int i = 1; i < args.size(); ++i)
    {
        const QString arg = args[i];
        if (arg == "--rough" || arg.startsWith("--rough="))
        {
            hasCliRough = true;
            break;
        }
    }

    if (!hasCliPrecise && !hasCliRough)
    {
        if (const char* envPrecise = std::getenv("NS3_VISUALIZER_PRECISE"))
        {
            const QString value = QString::fromLocal8Bit(envPrecise).trimmed().toLower();
            precise = (value == "1" || value == "true" || value == "yes");
        }
        if (const char* envRate = std::getenv("NS3_VISUALIZER_SAMPLE_RATE"))
        {
            bool ok = false;
            const int value = QString::fromLocal8Bit(envRate).toInt(&ok);
            if (ok && value > 1)
            {
                precise = false;
                sampleRate = value;
            }
        }
    }

    g_ppduViewConfig.preciseMode.store(precise);
    g_ppduViewConfig.absoluteRate.store(precise);
    g_ppduViewConfig.sampleRate.store(precise ? 1 : std::max(1, sampleRate));
}
} // namespace

int main(int argc, char *argv[])
{
    // 开启高 DPI 自适应
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);

    QFile f(":/qss/dark.qss");
    // QFile f("qss/dark.qss");

    if (f.open(QFile::ReadOnly))
    {
        a.setStyleSheet(f.readAll());
        f.close();
    }

    a.setStyleSheet(
        a.styleSheet() +
        "\nQToolTip {"
        "background-color: rgba(255, 255, 255, 244);"
        "color: #243142;"
        "border: 1px solid #D7E0EA;"
        "border-radius: 12px;"
        "padding: 10px 12px;"
        "font-size: 13px;"
        "}"
    );

    const QStringList args = a.arguments();
    ApplySamplingConfig(args);
    if (args.contains("--timeline-only"))
    {
        TimelineWindow w;
        w.startReader();
        w.showMaximized();
        return a.exec();
    }

    MainWindow w(VisualizerMode::FullGui);

    return a.exec();
}
