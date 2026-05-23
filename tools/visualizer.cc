#include "ns3/core-module.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using namespace ns3;

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    namespace fs = std::filesystem;

    fs::path cwd = fs::current_path();
    fs::path appPath = cwd / "build" / "Ns3VisualizerApp";
    if (!fs::exists(appPath))
    {
        appPath = cwd / "Ns3VisualizerApp";
    }

    const std::string command = appPath.string();
    const int ret = std::system(command.c_str());
    if (ret != 0)
    {
        std::cerr << "Failed to launch visualizer app: " << appPath << std::endl;
        return 1;
    }
    return 0;
}
