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
    fs::path appPath = cwd / "build" / "WiFiVizApp";
    if (!fs::exists(appPath))
    {
        appPath = cwd / "WiFiVizApp";
    }

    const int ret = std::system(appPath.string().c_str());
    if (ret != 0)
    {
        std::cerr << "Failed to launch visualizer app: " << appPath << std::endl;
        return 1;
    }
    return 0;
}
