#include "ns3/core-module.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace ns3;

namespace
{

std::filesystem::path
WiFiVizExecutableName()
{
#ifdef _WIN32
    return "WiFiVizApp.exe";
#else
    return "WiFiVizApp";
#endif
}

std::filesystem::path
FindWiFiVizApp()
{
    namespace fs = std::filesystem;
    const fs::path cwd = fs::current_path();
    const fs::path appName = WiFiVizExecutableName();
    const std::vector<fs::path> candidates = {
        cwd / "build" / appName,
        cwd / appName,
#ifdef __APPLE__
        cwd / "build" / "WiFiVizApp.app" / "Contents" / "MacOS" / "WiFiVizApp",
        cwd / "WiFiVizApp.app" / "Contents" / "MacOS" / "WiFiVizApp",
#endif
    };

    for (const auto& candidate : candidates)
    {
        if (fs::exists(candidate))
        {
            return candidate;
        }
    }
    return {};
}

int
RunWiFiVizApp(const std::filesystem::path& appPath)
{
#ifdef _WIN32
    std::wstring commandLine = L"\"" + appPath.wstring() + L"\"";
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    const BOOL ok = CreateProcessW(nullptr,
                                   commandLine.data(),
                                   nullptr,
                                   nullptr,
                                   FALSE,
                                   0,
                                   nullptr,
                                   nullptr,
                                   &startupInfo,
                                   &processInfo);
    if (!ok)
    {
        return 1;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return static_cast<int>(exitCode);
#else
    const std::string path = appPath.string();
    execl(path.c_str(), path.c_str(), static_cast<char*>(nullptr));
    std::cerr << "Failed to launch visualizer app: " << appPath << std::endl;
    return 1;
#endif
}

} // namespace

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    const auto appPath = FindWiFiVizApp();
    if (appPath.empty())
    {
        std::cerr << "Failed to find WiFiVizApp in the current ns-3 directory" << std::endl;
        return 1;
    }

    return RunWiFiVizApp(appPath);
}
