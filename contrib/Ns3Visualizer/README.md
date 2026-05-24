# WiFi-Visualizer for ns-3

[English](README.md) | [中文](utils/doc/README_ch.md)

WiFi-Visualizer is a plugin-style Qt visualization module for ns-3 Wi-Fi simulations. It provides two workflows: a full graphical workflow for building, running, and inspecting simulations, and a one-click workflow that attaches the visualizer to an existing ns-3 script.

The project is distributed as `contrib/Ns3Visualizer` only. It does not include the ns-3 source tree and does not require destructive modifications to ns-3 core files.

![Overview](img/overview.png)

## Key Features

- Full-mode simulation configuration dashboard for building Wi-Fi scenarios without writing repetitive C++ code.
- One-click visualization for existing ns-3 scripts through `QNs3Helper::MaybeEnableVisualizer`.
- PPDU timeline, channel-state timeline, and PHY-state timeline for micro-level PHY/MAC inspection.
- Throughput, delay, CDF, node throughput, frame composition, RX outcome, MCS distribution, and PHY-state pie charts.
- Interactive zoom, pan, selection, detail panels, output viewer, and image export.
- JSON-based scenario description and automatic ns-3 script generation.

## Repository Layout

```text
contrib/Ns3Visualizer/
├── CMakeLists.txt
├── README.md
├── doc/                         # ns-3 module documentation
├── examples/                    # Example ns-3 scripts using the visualizer helper
├── helper/                      # QNs3Helper and visualization attachment logic
├── model/                       # Trace capture, shared-memory records, and utilities
├── Simulation/
│   └── Default/                 # Built-in demo scenarios shown in the GUI
├── test/                        # ns-3 test skeleton
├── ui/                          # Qt application and ns3-script-generator
└── utils/doc/                   # English and Chinese README mirrors
```

Generated scenario folders under `Simulation/Designed/`, local `scratch/` scripts, build directories, `.codex`, and the sibling `contrib/nr` module are not part of this tool and should not be committed.

## Requirements

- ns-3.46 or a compatible newer ns-3 release.
- Qt 5.15+ or Qt 6.
- CMake 3.16+.
- C++17 compiler.
- Linux is the primary tested environment.

## Installation

Place this repository under an ns-3 source tree:

```bash
cd /path/to/ns-3.46/contrib
git clone https://github.com/z14212638-eng/Ns3-based-Visualization.git Ns3Visualizer
cd ..
./ns3 configure
./ns3 build
```

The build creates the Qt application `build/Ns3VisualizerApp` and the script generator `build/ns3-script-generator`.

## Full Mode

Full mode starts the visualizer as a standalone ns-3 scratch target:

```bash
./ns3 run visualizer
```

To support this command without modifying ns-3 core source files, copy the launcher script into `scratch/`:

```bash
cp contrib/Ns3Visualizer/tools/visualizer.cc scratch/visualizer.cc
```

If `contrib/Ns3Visualizer/tools/visualizer.cc` is not present in your checkout, create `scratch/visualizer.cc` with the launcher shown in [To Developers](#to-developers). This extra scratch file is intentional: ns-3 only runs targets it knows about, and adding `visualizer` as a built-in target would require changing ns-3 build/source files. This project keeps ns-3 itself untouched and behaves like a plugin.

![Full Mode](img/full-mode.gif)

Full mode workflow:

1. Open the visualizer with `./ns3 run visualizer`.
2. Configure the building, nodes, traffic flows, PHY/MAC parameters, and simulation options.
3. Click `Generate` to generate JSON records and a standalone ns-3 C++ script.
4. The tool builds/runs the generated target and opens the visualization dashboard.
5. Use the output viewer button to inspect read-only ns-3 build/run logs.

## One-Click Mode

One-click mode is for existing ns-3 scripts. The default command is:

```bash
./ns3 run "<target> --enable-visualizer=1"
```

For example:

```bash
./ns3 run "QNs3-example --enable-visualizer=1"
./ns3 run "wifi-txop-aggregation --enable-visualizer=1 --precise=1 --rough=1"
```

The script must expose and use a visualizer flag:

```cpp
bool enableVisualizer = false;
bool precise = true;
uint32_t rough = 1;

CommandLine cmd(__FILE__);
cmd.AddValue("enable-visualizer", "Enable WiFi-Visualizer timeline capture", enableVisualizer);
cmd.AddValue("precise", "Capture every PPDU without down-sampling", precise);
cmd.AddValue("rough", "Down-sample PPDU timeline to 1/N", rough);
cmd.Parse(argc, argv);

Ptr<SniffUtils> visualizer = QNs3Helper::MaybeEnableVisualizer(
    enableVisualizer,
    allDevices,
    simulationTimeSeconds,
    precise);
```

Passing `--enable-visualizer=1` is not mandatory if your script sets `enableVisualizer = true` directly before calling `MaybeEnableVisualizer`. In that case, this also works:

```bash
./ns3 run <target>
```

The command-line flag is recommended for reusable examples because users can enable or disable visualization without editing the script.

![One-Click Mode](img/one-click-mode.gif)

## UI Modules

### Start Page

The start page selects between full mode and one-click mode. It also lists built-in scenarios and keeps the entry point simple for users who only want to inspect a default simulation.

![Start Page](img/start-page.png)

### Simulation Configuration Dashboard

The configuration dashboard is used in full mode. The screenshot below shows only part of the available configuration UI.

![Configuration Dashboard](img/config-dashboard.png)

Main configuration areas:

- Building: building size, wall settings, room layout, simulation duration, and environment-level parameters.
- Layout canvas: AP/STA insertion, deletion, dragging, position synchronization, map enlargement, and spatial verification.
- Node configuration: AP and STA PHY/MAC settings, wireless standard, channel, frequency, power, antenna, mobility, RTS/CTS, aggregation, beacon, and EDCA/QoS options.
- Flow configuration: source/destination nodes, UDP-like and bulk-transfer traffic models, start/stop time, packet size, offered load, and rate-related parameters.
- Script generation: exports JSON configuration files and generates an ns-3 C++ script through `ns3-script-generator`.

### Result Dashboard

The result dashboard combines timeline inspection, statistical charts, detail panels, and output viewing.

![Result Overview](img/result-overview.png)

Timeline views:

- PPDU Timeline: visualizes each PPDU as a duration-aware item, separates overlapping transmissions into lanes, and exposes metadata such as sender, receiver, frame type, MCS, SNR, aggregation, delay, and RX result.
- Channel-State Timeline: reconstructs channel occupancy as IDLE, BUSY, and COLLISION intervals.
- PHY-State Timeline: displays PHY states such as IDLE, TX, RX, CCA_BUSY, SWITCHING, SLEEP, and OFF.

Statistical charts:

- Throughput Chart: time-varying throughput and average throughput.
- Delay Charts: queueing delay and MAC end-to-end delay, each with time-series and CDF views.
- Frame Composition Chart: proportions of data, control, and management frames.
- Node Throughput Chart: node-level throughput contribution.
- RX-Outcome Chart: successful receptions, collisions, and decoding failures.
- MCS Distribution Chart: usage frequency of modulation and coding schemes.
- PHY State Pie Chart: aggregate state-duration share with per-PHY average duration.

### Output Viewer

The output viewer opens build/run logs in a read-only industrial-gray panel. It is intentionally separated from the main dashboard so result inspection is not crowded by terminal output.

![Output Viewer](img/output-viewer.png)

## Data Flow

```text
GUI configuration
      ↓
JSON files under Simulation/Designed/<scenario>/
      ↓
ns3-script-generator
      ↓
scratch/<generated-target>.cc
      ↓
ns-3 build/run
      ↓
shared-memory trace records
      ↓
Qt visualization dashboard
```

One-click mode skips the GUI configuration and script generation stages. It attaches trace capture directly inside the user script.

## To Developers

Recommended launcher for `scratch/visualizer.cc`:

```cpp
#include "ns3/core-module.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using namespace ns3;

int main(int argc, char* argv[])
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

    const int ret = std::system(appPath.string().c_str());
    if (ret != 0)
    {
        std::cerr << "Failed to launch visualizer app: " << appPath << std::endl;
        return 1;
    }
    return 0;
}
```

Development notes:

- Keep ns-3 core untouched. Prefer scratch launchers, contrib helpers, and generated scripts.
- Do not commit generated `Simulation/Designed/*` experiments unless they are intentional examples.
- Do not commit local `.codex`, build directories, `scratch/`, binary packages, or sibling modules such as `contrib/nr`.
- Keep `ui/CMakeLists.txt` and `ui/Ns3Visualizer.pro` synchronized when adding Qt source files.
- When adding a new chart, document its statistical definition and unit in the UI and README.

## License

Released under the MIT License.

## Authors

- Kai Zhang, u202414527@hust.edu.cn
- Chengxiang Mi, michengxiang@hust.edu.cn

## Acknowledgements

- ns-3 Simulator
- Qt Framework
- NetAnim, for UI inspiration
