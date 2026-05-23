# ns3-Visualizer

`ns3-Visualizer` is a contrib module for ns-3 that provides a Qt-based workflow for Wi-Fi simulation configuration and PHY/MAC PPDU-level trace visualization.  This repository intentionally contains only the contrib module, not the full ns-3 source tree.

![Overview](img/overview.gif)

## Repository Scope

This repository is meant to be copied into an existing ns-3 checkout:

```text
ns-3.46/
└── contrib/
    └── Ns3Visualizer/
```

It does not include:

- the ns-3 core source tree,
- generated `scratch/` programs,
- local `.codex` configuration,
- AppImage packaging output,
- `contrib/nr` or any unrelated contrib module.

## Features

- GUI-driven Wi-Fi scenario configuration for buildings, APs, STAs, mobility, PHY/MAC parameters, EDCA/QoS, antenna settings, aggregation, RTS/CTS, and traffic flows.
- JSON-backed scenario storage and standalone ns-3 C++ script generation.
- Shared-memory transport for real-time PPDU, PHY-state, MAC-delay, and RX-outcome records.
- Interactive visualization for PPDU timing, channel state, PHY state, throughput, latency, frame composition, node throughput, RX outcome, and MCS distribution.
- Timeline zooming, panning, range selection, PPDU detail inspection, and process-output viewing.

![Configuration Dashboard](img/configuration-dashboard.png)

![Visualization Dashboard](img/visualization-dashboard.gif)

## Directory Layout

```text
contrib/Ns3Visualizer/
├── CMakeLists.txt
├── model/                  # ns-3 model classes and trace collection logic
├── helper/                 # helper APIs for enabling visualizer tracing
├── examples/               # example ns-3 programs
├── test/                   # ns-3 test skeleton
├── Simulation/Default/     # built-in example scenarios for the GUI
├── doc/                    # ns-3 module documentation
├── utils/doc/              # extended user notes
└── ui/                     # Qt visualizer frontend and script generator
```

## Requirements

- ns-3.46 or a compatible recent ns-3 version
- CMake and a C++17-capable compiler for the Qt frontend
- C++23-capable compiler support for the ns-3 contrib module build settings
- Qt 6 preferred; Qt 5.15 may work with minor adjustments
- Boost Interprocess
- Linux is the primary tested environment

## Installation

Clone this repository and copy the contrib module into an ns-3 source tree:

```bash
git clone https://github.com/z14212638-eng/Ns3-based-Visualization.git
cp -r Ns3-based-Visualization/contrib/Ns3Visualizer /path/to/ns-3.46/contrib/
```

Then configure and build ns-3:

```bash
cd /path/to/ns-3.46
./ns3 configure
./ns3 build
```

The build adds the `Ns3Visualizer` contrib module and builds the Qt frontend target when Qt is available.

## Running

Launch the full GUI:

```bash
cd /path/to/ns-3.46
./build/Ns3VisualizerApp
```

The GUI can create or load a project, generate a standalone scratch script, build it, run it with visualizer tracing enabled, and display the resulting PPDU-level records.

For timeline-only mode, which is useful when an ns-3 script launches the viewer:

```bash
./build/Ns3VisualizerApp --timeline-only
```

## Scenario Configuration Workflow

![Scenario Builder](img/scenario-builder.gif)

1. Configure the building and global simulation duration.
2. Add AP and STA nodes and adjust their positions in the layout view.
3. Configure node-specific PHY/MAC, mobility, antenna, aggregation, RTS/CTS, and EDCA parameters.
4. Add traffic flows between selected AP/STA endpoints.
5. Generate the standalone ns-3 script and run the simulation from the GUI.

## Visualization Workflow

![PPDU Timeline](img/ppdu-timeline.gif)

The visualizer consumes shared-memory records emitted by ns-3 and updates several linked views:

- PPDU timeline for individual transmissions and overlaps.
- Channel-state timeline for IDLE/BUSY/COLLISION intervals.
- PHY-state timeline for IDLE/TX/RX/CCA_BUSY and related states.
- Throughput and delay charts for performance diagnosis.
- Frame composition, node throughput, RX-outcome, and MCS-distribution charts for statistical summaries.

![Statistics Panels](img/statistics-panels.png)

## Enabling Tracing in Custom Scripts

Generated scripts already include the required visualizer hooks.  For custom scripts, include the helper and enable visualizer tracing on the Wi-Fi devices:

```cpp
#include "ns3/QNs3-helper.h"

// after AP and STA NetDeviceContainers are created
NetDeviceContainer allDevices = QNs3Helper::MergeDevices(apDevices, staDevices);
QNs3Helper::ConfigureVisualizerSampling(/* precise */ true, /* rough */ 1);
Ptr<SniffUtils> sniffer =
    QNs3Helper::MaybeEnableVisualizer(enableVisualizer, allDevices, simulationTime, true);
```

Then run the simulation with:

```bash
./ns3 run "your-script --enable-visualizer=1 --precise=1 --rough=1"
```

## Image Assets

The README references the following files.  They are intentionally expected under `img/` so screenshots and GIFs can be updated without changing Markdown:

```text
img/overview.gif
img/configuration-dashboard.png
img/visualization-dashboard.gif
img/scenario-builder.gif
img/ppdu-timeline.gif
img/statistics-panels.png
```

## Development Notes

- Keep generated projects under ns-3 runtime directories, not in this repository.
- Do not commit AppImage extraction directories such as `AppDir/` or `squashfs-root/`.
- Do not commit local `.codex`, build caches, or generated scratch programs.
- This repository should remain scoped to `contrib/Ns3Visualizer`.

## License

This project follows the ns-3 contrib-module style.  See the source files and the ns-3 license terms for details.
