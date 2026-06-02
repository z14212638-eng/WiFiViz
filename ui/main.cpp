#include "mainwindow.h"
#include "timeline_window.h"
#include "visualizer_mode.h"
#include "visualizer_config.h"
#include <QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QProcess>
#include <QFile>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace
{
constexpr int kDesignWidth = 2560;
constexpr int kDesignHeight = 1440;

struct ScreenSize
{
    int width = 0;
    int height = 0;
};

std::optional<std::string>
RunCommand(const char* command)
{
#ifdef Q_OS_LINUX
    std::array<char, 256> buffer{};
    std::string output;

    FILE* pipe = popen(command, "r");
    if (!pipe)
    {
        return std::nullopt;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
    {
        output += buffer.data();
    }

    const int status = pclose(pipe);
    if (status != 0 || output.empty())
    {
        return std::nullopt;
    }
    return output;
#else
    return std::nullopt;
#endif
}

std::optional<ScreenSize>
BestActiveMonitorFromXrandr()
{
    const auto output = RunCommand("xrandr --listactivemonitors 2>/dev/null");
    if (!output)
    {
        return std::nullopt;
    }

    const std::regex geometry(R"(\b([1-9][0-9]*)/[0-9]+x([1-9][0-9]*)/[0-9]+\+[0-9]+\+[0-9]+\b)");
    std::optional<ScreenSize> best;
    std::istringstream lines(*output);
    std::string line;

    while (std::getline(lines, line))
    {
        std::smatch match;
        if (!std::regex_search(line, match, geometry))
        {
            continue;
        }

        ScreenSize size{std::stoi(match[1].str()), std::stoi(match[2].str())};
        if (line.find('*') != std::string::npos)
        {
            return size;
        }

        if (!best || (size.width * size.height) > (best->width * best->height))
        {
            best = size;
        }
    }

    return best;
}

std::optional<ScreenSize>
BestConnectedScreenFromXrandr()
{
    const auto output = RunCommand("xrandr --current 2>/dev/null");
    if (!output)
    {
        return std::nullopt;
    }

    const std::regex geometry(R"(\b([1-9][0-9]*)x([1-9][0-9]*)[+-][0-9]+[+-][0-9]+\b)");
    std::optional<ScreenSize> best;
    std::istringstream lines(*output);
    std::string line;

    while (std::getline(lines, line))
    {
        if (line.find(" connected") == std::string::npos)
        {
            continue;
        }

        std::smatch match;
        if (!std::regex_search(line, match, geometry))
        {
            continue;
        }

        ScreenSize size{std::stoi(match[1].str()), std::stoi(match[2].str())};
        if (line.find(" primary ") != std::string::npos)
        {
            return size;
        }

        if (!best || (size.width * size.height) > (best->width * best->height))
        {
            best = size;
        }
    }

    return best;
}

std::optional<ScreenSize>
ScreenSizeFromXdpyinfo()
{
    const auto output = RunCommand("xdpyinfo 2>/dev/null");
    if (!output)
    {
        return std::nullopt;
    }

    const std::regex dimensions(R"(dimensions:\s+([1-9][0-9]*)x([1-9][0-9]*)\s+pixels)");
    std::smatch match;
    if (!std::regex_search(*output, match, dimensions))
    {
        return std::nullopt;
    }

    return ScreenSize{std::stoi(match[1].str()), std::stoi(match[2].str())};
}

std::optional<double>
ScaleFromEnvironment()
{
    const QByteArray value = qgetenv("WIFIVIZ_UI_SCALE");
    if (value.isEmpty())
    {
        return std::nullopt;
    }

    char* end = nullptr;
    const double scale = std::strtod(value.constData(), &end);
    if (end == value.constData() || scale <= 0.0)
    {
        return std::nullopt;
    }
    return scale;
}

void
ApplyDesignScale()
{
    const QString disabled = qEnvironmentVariable("WIFIVIZ_DISABLE_AUTO_SCALE").trimmed();
    if (!disabled.isEmpty())
    {
        const QString value = disabled.toLower();
        if (value == "1" || value == "true")
        {
            return;
        }
    }

    if (!ScaleFromEnvironment() && qEnvironmentVariableIsSet("QT_SCALE_FACTOR"))
    {
        return;
    }

    std::optional<double> scale = ScaleFromEnvironment();
    if (!scale)
    {
        std::optional<ScreenSize> screen = BestActiveMonitorFromXrandr();
        if (!screen)
        {
            screen = BestConnectedScreenFromXrandr();
        }
        if (!screen)
        {
            screen = ScreenSizeFromXdpyinfo();
        }

        if (!screen)
        {
            return;
        }

        scale = std::min(static_cast<double>(screen->width) / kDesignWidth,
                         static_cast<double>(screen->height) / kDesignHeight);
    }

    if (*scale <= 0.0)
    {
        return;
    }

    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("1"));
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", QByteArray("0"));
    qputenv("QT_SCALE_FACTOR", QByteArray::number(*scale, 'f', 4));
}

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
        if (qEnvironmentVariableIsSet("WIFIVIZ_PRECISE"))
        {
            const QString value = qEnvironmentVariable("WIFIVIZ_PRECISE").trimmed().toLower();
            precise = (value == "1" || value == "true" || value == "yes");
        }
        if (qEnvironmentVariableIsSet("WIFIVIZ_SAMPLE_RATE"))
        {
            bool ok = false;
            const int value = qEnvironmentVariableIntValue("WIFIVIZ_SAMPLE_RATE", &ok);
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
    ApplyDesignScale();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // 开启高 DPI 自适应
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

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
