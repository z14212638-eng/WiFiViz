# WiFiViz

`WiFiViz` is an ns-3 contrib module for building, running, and analyzing
Wi-Fi simulations through a Qt-based graphical workflow. It combines scenario
configuration, JSON persistence, script generation, ns-3 execution, shared-memory
trace collection, and interactive PHY/MAC visualization in one tool.

This repository is the standalone `wifiviz` contrib module. The tool name is
`ns3-WiFiViz`, and the Qt application remains `WiFiVizApp`, but the module
directory and CMake module name are lowercase `wifiviz`.

![Project overview](doc/figures/visualization-dashboard.png)

## Table of Contents

- [Package Layout](#package-layout)
- [Main Capabilities](#main-capabilities)
- [Requirements](#requirements)
- [Installation](#installation)
- [Build](#build)
- [Run Mode 1: Simple Script Mode](#run-mode-1-simple-script-mode)
- [Run Mode 2: Parameter Script Mode](#run-mode-2-parameter-script-mode)
- [Run Mode 3: Full GUI Mode](#run-mode-3-full-gui-mode)
- [UI Guide](#ui-guide)
- [Visualization Guide](#visualization-guide)
- [Project and Data Files](#project-and-data-files)
- [Module Layout](#module-layout)
- [Troubleshooting](#troubleshooting)

## Package Layout

After cloning this repository, the important paths are:

```text
wifiviz/
├── CMakeLists.txt
├── README.md
├── doc/
├── examples/
├── helper/
├── model/
├── test/
├── ui/
└── Simulation/
```

Install the repository itself as `/path/to/ns-3.46/contrib/wifiviz`.
The optional full-GUI launcher is registered as the module example
`wifiviz-visualizer`.

## Main Capabilities

- Full graphical configuration of Wi-Fi simulations, including building
  geometry, AP/STA placement, PHY/MAC parameters, mobility, antenna, EDCA/QoS,
  aggregation, RTS/CTS, beacon behavior, and traffic flows.
- JSON-based scenario storage. The GUI writes `General.json`, AP JSON files,
  and STA JSON files before generating the ns-3 script.
- Automatic standalone C++ script generation under ns-3 `scratch/`.
- Full GUI workflow that generates, builds, runs, and visualizes a simulation.
- Simple script workflow for users who already have their own ns-3 script.
- Shared-memory trace transport from ns-3 to the Qt application.
- PPDU-level timeline visualization, channel-state view, PHY-state view, detail
  inspection, throughput charts, delay charts, frame composition, node
  throughput, RX outcome, MCS distribution, PHY-state pie chart, delay CDF
  view, and simulation output viewer.

![Full GUI workflow](doc/figures/simulation-config-overview.png)

## Requirements

WiFiViz is developed and tested as an ns-3.46 contrib module on Linux. The
repository must be installed as `contrib/wifiviz` inside an ns-3 source tree;
some GUI paths and helper launch commands intentionally use that module path.

Required build environment:

- Linux with a graphical desktop session for the Qt viewer. X11 or Wayland is
  fine. For headless SSH/server runs, use script mode with `launchViewer=false`
  or set up display forwarding before launching `WiFiVizApp`.
- ns-3.46 source tree configured with CMake.
- CMake 3.16 or newer. The Qt frontend uses CMake AUTOMOC, AUTOUIC, and AUTORCC.
- A compiler matching ns-3's C++ standard for the ns-3 contrib module.
- A C++17-capable compiler for the Qt frontend and
  `wifiviz-script-generator`. In practice this is normally the same compiler.

Required ns-3 modules:

- `core`, `network`, `internet`, `mobility`, `wifi`, `applications`,
  `buildings`, `csma`, `point-to-point`, and `aodv`.

Required third-party libraries and tools:

- Qt Core, Gui, and Widgets development packages. Qt 6 is preferred; Qt 5.15 is
  the fallback supported by the CMake files.
- Boost 1.71 or newer headers, specifically Boost Interprocess, for
  shared-memory transport between ns-3 and the Qt viewer.
- nlohmann JSON headers and CMake package. The module parser includes
  `nlohmann/json.hpp`, and the script generator looks for
  `nlohmann_json >= 3.2.0`.
- The GUI launches ns-3 and the timeline viewer directly through platform
  process APIs; no POSIX shell wrapper is required for normal operation.
- Standard ns-3 build tools such as `git`, `python3`, `pkg-config`, CMake, and
  Ninja or Make.

Typical Ubuntu 22.04/24.04 packages for Qt 6:

```bash
sudo apt update
sudo apt install -y \
  build-essential git python3 pkg-config cmake ninja-build \
  qt6-base-dev qt6-base-dev-tools \
  libboost-all-dev nlohmann-json3-dev
```

If your distribution only provides Qt 5.15:

```bash
sudo apt install -y \
  build-essential git python3 pkg-config cmake ninja-build \
  qtbase5-dev qtbase5-dev-tools \
  libboost-all-dev nlohmann-json3-dev
```

Notes:

- `libboost-all-dev` can be replaced with a smaller Boost development package
  if it provides Boost 1.71 or newer and includes Boost Interprocess headers on
  your distribution.
- The project does not use Qt Charts, Qt OpenGL, Qt Network, Qt Multimedia, or
  Qt Concurrent. Installing `qt6-base-dev` or `qtbase5-dev` is enough for the
  current frontend.
- The generator target links `stdc++fs` only when building with GCC older than
  9.0 for filesystem compatibility.

## Installation

Clone this repository directly into the ns-3 `contrib/` directory. The clone
name must be `wifiviz` so ns-3 sees the expected module path:

```bash
cd /path/to/ns-3.46/contrib
git clone <repo-url> wifiviz
```

The full-GUI launcher is registered as the module example
`wifiviz-visualizer`, so it can be started directly with `./ns3 run
wifiviz-visualizer`. No copy into `scratch/` is required.

The final path must be:

```text
/path/to/ns-3.46/contrib/wifiviz
```

## Build

Configure and build ns-3 from the ns-3 root directory. Enable examples so the
`wifiviz-visualizer` launcher target is built:

```bash
cd /path/to/ns-3.46
./ns3 configure --enable-examples
./ns3 build
```

The build should produce:

```text
build/WiFiVizApp
build/wifiviz-script-generator
```

`WiFiVizApp` is the Qt frontend. `wifiviz-script-generator` is used by the
GUI to convert JSON scenario files into a standalone ns-3 C++ script.

## Run Mode 1: Simple Script Mode

Simple mode is the recommended path for most users. You keep writing your ns-3
script in the normal ns-3 style, then add a small WiFiViz block near the end of
the script. All WiFiViz options are written directly as function parameters in
the script, so you do not need to add WiFiViz command-line parsing.

First include the aggregate header:

```cpp
#include "ns3/wifiviz.h"
```

After your Wi-Fi AP and STA `NetDeviceContainer`s have been created, merge the
devices you want to trace. Usually this means all AP devices plus all STA
devices:

```cpp
using namespace ns3;

// Existing containers from your script.
NetDeviceContainer apDevices = ...;
NetDeviceContainer staDevices = ...;

NetDeviceContainer allDevices = WiFiVizHelper::MergeDevices(apDevices, staDevices);
```

Then pass `allDevices`, `enableViz`, the simulation duration, and
`launchViewer` directly into `MaybeEnableVisualizer`:

```cpp
bool enableViz = true;
double simulationTimeSeconds = 10.0;
bool launchViewer = true;

Ptr<SniffUtils> sniffer =
    WiFiVizHelper::MaybeEnableVisualizer(enableViz,
                                         allDevices,
                                         simulationTimeSeconds,
                                         launchViewer);
```

Meaning of the parameters:

- `enableViz`: set to `true` to collect WiFiViz records.
- `allDevices`: the AP/STA Wi-Fi devices to trace.
- `simulationTimeSeconds`: total simulation duration in seconds.
- `launchViewer`: set to `true` to open the timeline viewer automatically.

If you want sampled visualization for a large simulation, configure sampling
directly before `MaybeEnableVisualizer`:

```cpp
bool precise = false;
uint32_t rough = 10;
WiFiVizHelper::ConfigureVisualizerSampling(precise, rough);
```

Then run the script normally. No WiFiViz command-line arguments are needed:

```bash
cd /path/to/ns-3.46
./ns3 run your-target
```

Use simple mode when the script is dedicated to visualization or when you do
not need to switch WiFiViz on and off from the terminal.

## Run Mode 2: Parameter Script Mode

Parameter mode is useful when you want the same script to run with or without
WiFiViz from the terminal. In this mode, your script defines command-line
parameters and then passes the parsed values into the same WiFiViz helper
functions.

```cpp
#include "ns3/wifiviz.h"

bool enableWiFiViz = false;
bool launchViewer = true;
bool precise = true;
uint32_t rough = 1;
double simulationTimeSeconds = 10.0;

CommandLine cmd(__FILE__);
cmd.AddValue("enable-wifiviz", "Enable WiFiViz timeline capture", enableWiFiViz);
cmd.AddValue("launch-viewer", "Launch the WiFiViz timeline viewer", launchViewer);
cmd.AddValue("precise", "Use precise PPDU visualization", precise);
cmd.AddValue("rough", "Sample one PPDU out of rough records when precise=false", rough);
cmd.AddValue("simulation-time", "Simulation duration in seconds", simulationTimeSeconds);
cmd.Parse(argc, argv);
```

After creating the AP/STA devices, use the parsed values:

```cpp
NetDeviceContainer allDevices = WiFiVizHelper::MergeDevices(apDevices, staDevices);

WiFiVizHelper::ConfigureVisualizerSampling(precise, rough);

Ptr<SniffUtils> sniffer =
    WiFiVizHelper::MaybeEnableVisualizer(enableWiFiViz,
                                         allDevices,
                                         simulationTimeSeconds,
                                         launchViewer);
```

Run it with parameters:

```bash
cd /path/to/ns-3.46
./ns3 run "your-target --enable-wifiviz=1 --launch-viewer=1 --precise=1 --rough=1 --simulation-time=10"
```

Run in sampled mode for heavy simulations:

```bash
./ns3 run "your-target --enable-wifiviz=1 --launch-viewer=1 --precise=0 --rough=10 --simulation-time=10"
```

What happens:

- `WiFiVizHelper::MaybeEnableVisualizer(..., true)` starts
  `build/WiFiVizApp --timeline-only` automatically.
- The script writes visualizer records to shared memory.
- The timeline-only viewer reads the records and shows the result dashboard.

## Run Mode 3: Full GUI Mode

Full mode starts the WiFiViz GUI first and lets the GUI generate a scratch
script from the configuration pages. This mode is useful for quickly trying the
graphical configuration workflow.

Start the GUI through ns-3:

```bash
cd /path/to/ns-3.46
./ns3 run wifiviz-visualizer
```

`wifiviz-visualizer` is the module example built from
`examples/wifiviz-visualizer.cc`. The launcher only starts `build/WiFiVizApp`
from the ns-3 root; it does not modify ns-3 internals. If you only need to
start the Qt application directly, use:

```bash
./build/WiFiVizApp
```

Full mode workflow:

1. Select the ns-3 directory on the welcome page.
2. Configure a simple Wi-Fi scenario in the GUI.
3. Click `Generate`.
4. The GUI writes JSON files, runs `build/wifiviz-script-generator`, creates a
   `scratch/*.cc` script, runs `./ns3 build`, and then runs the generated
   target with WiFiViz enabled.

Current limitation: the full-mode script generator currently generates only
simple scripts. For complex experiments, custom helpers, advanced traffic
patterns, or nonstandard ns-3 workflows, the generated script may contain
unexpected errors or incomplete logic. For reproducible research scripts, the
recommended workflow is to write your own ns-3 script using
`examples/wifiviz-basic-example.cc` as a template, then use simple mode to
visualize the final result.

## UI Guide

This section describes every major UI page and panel.

### 1. Welcome and NS-3 Path Page

![Welcome and path page](doc/figures/welcome-ns3-path.png)

The welcome page asks for the ns-3 root directory. The selected directory is
validated before the user can enter the simulation workflow. The toolbar also
contains an `NS-3 Path` action so the working directory can be changed later.

Use this page to:

- set `/path/to/ns-3.46`,
- verify that the GUI can locate ns-3 files,
- prepare the default scene browser and script generator paths.

### 2. Scenario Library Page

![Scenario library](doc/figures/scene-library.png)

The scenario page provides three entry points:

- `Simple`: built-in simple examples under
  `contrib/wifiviz/Simulation/Default/Simple`.
- `Complex`: built-in complex examples under
  `contrib/wifiviz/Simulation/Default/Complex`.
- `Scratch`: readable `*.cc` files under ns-3 `scratch/`.

The page shows a preview image and Markdown description when a default scene
contains `info.md` and an image asset. `Simulation Selected` runs the selected
scene through the visualizer path. `Config Simulation` opens the full
configuration dashboard. `Load from file` loads a saved project JSON.

### 3. Simulation Configuration Dashboard

![Simulation configuration overview](doc/figures/simulation-config-overview.png)

The configuration dashboard is the central page for creating a new scenario.
It contains global building settings, AP/STA creation controls, node tables, an
interactive layout canvas, validation/update controls, and the final
`Generate` action.

Use this page to:

- set global simulation duration,
- configure the building size and wall model,
- add or delete AP and STA nodes,
- drag nodes on the map and synchronize their coordinates,
- open AP/STA configuration pages,
- right-click/select a node to edit its traffic in the right sidebar,
- write the current configuration into JSON,
- generate, build, run, and switch to visualization.

### 4. Building and Layout Controls

![Building configuration](doc/figures/building-config.png)

Building controls define the physical simulation environment:

- X/Y/Z range,
- building type,
- exterior wall type,
- simulation time.

![Interactive layout map](doc/figures/layout-map.png)

The layout map renders AP and STA positions inside the configured building
boundary. Nodes can be moved interactively. The enlarged map view supports
zooming, panning, and synchronized node repositioning.

### 5. AP Configuration Page

![AP configuration](doc/figures/ap-config.png)

The AP page configures access-point-specific parameters. It includes:

- node position and mobility,
- Wi-Fi standard, channel, center frequency, bandwidth, guard interval, PHY
  model, transmit power, receiver sensitivity, CCA sensitivity, and CCA
  threshold,
- SSID and AP MAC behavior,
- beacon interval, beacon rate, and beacon jitter,
- RTS/CTS enable flag and threshold,
- rate-control algorithm and queue type,
- QoS and EDCA access-category settings,
- A-MSDU and A-MPDU aggregation settings,
- antenna settings,
- AP-owned traffic flows.

The page stores values into the AP JSON configuration and returns to the
simulation dashboard after the AP is accepted.

### 6. STA Configuration Page

![STA configuration](doc/figures/sta-config.png)

The STA page mirrors the AP page for station nodes and adds station-specific
association controls:

- active probing,
- maximum missed beacons,
- probe request timeout,
- association retry behavior.

It also supports mobility, PHY/MAC settings, RTS/CTS, rate control, QoS/EDCA,
aggregation, antenna, and STA-owned traffic flows.

### 7. EDCA/QoS Dialog

![EDCA configuration](doc/figures/edca-config.png)

The EDCA dialog configures per-access-category contention parameters:

- `AC_VO`, `AC_VI`, `AC_BE`, `AC_BK`,
- CWmin,
- CWmax,
- AIFSN,
- TXOP limit.

These values are applied to AP or STA QoS MAC configuration when QoS is enabled.

### 8. Antenna Dialog

![Antenna configuration](doc/figures/antenna-config.png)

The antenna dialog configures antenna model information used by the node
configuration. It supports selecting antenna type and related gain/beamwidth
parameters where applicable.

### 9. Node Traffic Sidebar

![Node traffic sidebar](doc/figures/node-traffic-panel.png)

The right sidebar contains a node traffic panel while the configuration page is
active. Right-click or select a node in the map to inspect flows owned by that
node. The panel supports:

- adding a flow,
- editing a selected flow,
- deleting a selected flow,
- clearing all flows for the node,
- selecting destinations from known AP/STA targets.

Changes are synchronized into the JSON model when the simulation configuration
is updated.

### 10. Flow Configuration Dialog

![Flow dialog](doc/figures/flow-dialog.png)

The flow dialog supports multiple traffic abstractions:

- `OnOff`: protocol, destination, start/stop time, ToS, data rate, packet size,
  on-time random variable, off-time random variable, and max bytes.
- `CBR`: protocol, destination, start/stop time, ToS, packet size, interval,
  and max packets.
- `Bulk`: protocol, destination, start/stop time, ToS, and max bytes.

Random-variable controls allow constant, uniform, exponential, and other
parameterized distributions where the UI exposes them.

## Visualization Guide

![Visualization dashboard](doc/figures/visualization-dashboard.png)

The result dashboard receives records from ns-3 through shared memory and
updates linked views in real time or after simulation completion.

### PPDU Timeline

The PPDU timeline draws each PPDU as a horizontal time-span item. Width
represents duration. Rows can be organized by sender, channel, or node/link
depending on the selected timeline mode. The view supports:

- wheel zooming,
- horizontal navigation,
- time-range selection,
- image export,
- legend display,
- hover tooltips,
- click-to-inspect PPDU details.

### Channel-State Timeline

![Channel-state timeline](doc/figures/channel-state-timeline.png)

The channel-state view reconstructs channel occupancy from PPDU start/end
timestamps. It classifies intervals as IDLE, BUSY, or COLLISION. This view is
useful for quickly locating high-contention periods and idle gaps.

### PHY-State Timeline

![PHY-state timeline](doc/figures/phy-state-timeline.png)

The PHY-state view displays radio state transitions such as IDLE, TX, RX,
CCA_BUSY, SWITCHING, SLEEP, and OFF. It helps explain why frames are delayed,
blocked, or overlapping with channel access activity.

### PPDU Detail Sidebar

![PPDU detail sidebar](doc/figures/ppdu-detail-sidebar.png)

Clicking a PPDU updates the right detail sidebar. The sidebar shows packet
metadata such as time range, sender, receiver, frame type, MCS, channel, SNR,
aggregation information, queueing delay, MAC end-to-end delay, and reception
outcome when those fields are available.

### Throughput Chart

![Throughput chart](doc/figures/throughput-chart.png)

The throughput chart shows temporal throughput samples calculated from
successfully received PPDU data. Values are rendered in Mbps. The chart also
draws an average line to show the overall trend of the current run.

### Delay Charts

![Delay charts](doc/figures/delay-charts.png)

The delay panel switches between:

- `Queueing Delay`: time spent waiting before transmission service,
- `MAC End-to-End Delay`: time from MAC queue entry to successful MAC-level
  completion/acknowledgement.

These charts are useful for identifying MAC congestion and contention-induced
service delay.

Each delay chart also provides `CDF View`:

- X-axis: delay in milliseconds.
- Y-axis: cumulative probability from 0 to 1.
- A curve closer to the left indicates lower overall delay.
- A steeper curve indicates more stable delay.

### Frame Mix Chart

![Frame mix chart](doc/figures/frame-mix-chart.png)

The frame mix chart summarizes the proportions of data, control, management,
and other observed frame categories. It is useful for checking protocol
overhead and verifying whether RTS/CTS, ACK, or management behavior appears as
expected.

### Node Throughput Chart

![Node throughput chart](doc/figures/node-throughput-chart.png)

The node throughput chart aggregates throughput contribution by AP/STA MAC
address and displays node-level share. It helps identify unfairness, traffic
concentration, and asymmetric topology effects.

### RX Outcome Chart

![RX outcome chart](doc/figures/rx-outcome-chart.png)

The RX outcome chart groups reception results into successful receptions,
collision-related failures, and other decoding failures. It should be read
together with the PPDU and channel-state timelines.

### MCS Distribution Chart

![MCS distribution chart](doc/figures/mcs-distribution-chart.png)

The MCS distribution chart summarizes modulation and coding scheme usage across
observed transmissions. It helps diagnose rate adaptation behavior and channel
quality.

### PHY State Pie Chart

![PHY state pie chart](doc/figures/phy-state-pie-chart.png)

The PHY state pie chart aggregates the duration share of IDLE, TX, RX,
CCA_BUSY, SWITCHING, SLEEP, OFF, and UNKNOWN states. The center label reports
the average observed duration per PHY. Tooltips also show aggregate duration,
per-PHY average duration, and percentage share.

### Output Window

![Output window](doc/figures/output-window.png)

The `Output` button opens a read-only industrial-gray output window. It shows
the `./ns3 build`, `./ns3 run`, generator, stdout, and stderr logs.

## Project and Data Files

Full GUI mode writes project data under:

```text
contrib/wifiviz/Simulation/Designed/Designed_<timestamp>/
├── GeneralJson/
│   └── General.json
├── ApConfigJson/
│   └── Ap_<id>.json
└── StaConfigJson/
    └── Sta_<id>.json
```

The generated ns-3 C++ script is written to:

```text
scratch/<generated-target>.cc
```

These files are generated automatically by the full GUI workflow. You can keep
them for later inspection or delete them after the experiment.

## Module Layout

```text
contrib/wifiviz/
├── CMakeLists.txt
├── model/
│   ├── wifiviz.h              # public aggregate header for user scripts
│   ├── QNs3.*                 # JSON config structures and parsing helpers
│   └── SniffUtils.*           # ns-3 trace collection and shared-memory writer
├── helper/
│   └── QNs3-helper.*          # implementation behind WiFiVizHelper
├── examples/                  # minimal user-script template
├── test/                      # ns-3 unit tests
├── Simulation/Default/        # built-in simple and complex GUI scenes
├── doc/                       # ns-3 module documentation
├── utils/doc/                 # additional notes
└── ui/
    ├── main.cpp               # GUI entry point and timeline-only mode
    ├── mainwindow.*           # page navigation and signal wiring
    ├── simu_config.*          # building/layout/generate/build/run workflow
    ├── ap_config.*            # AP parameter UI
    ├── node_config.*          # STA parameter UI
    ├── flow_dialog.*          # traffic flow editor
    ├── node_traffic_panel.*   # right-sidebar node traffic editor
    ├── ppdu_timeline_view.*   # PPDU/channel/PHY timeline renderer
    ├── *_chart.*              # throughput, delay, RX, MCS, and statistics charts
    ├── process_terminal.*     # output viewer
    └── utils/
        └── wifiviz-script-generator.cc
```

## Troubleshooting

### `build/WiFiVizApp` does not exist

Run:

```bash
cd /path/to/ns-3.46
./ns3 configure
./ns3 build
```

Check whether Qt development packages are installed.

### `build/wifiviz-script-generator` does not exist

The generator is built together with the Qt frontend. Re-run `./ns3 build` and
check the CMake output for Qt or compiler errors.

### Full GUI mode generates a script but no packets appear

Check the output window. Common causes are:

- simulation time is zero,
- no AP or no STA was configured,
- no traffic flow starts during the simulation,
- the generated script failed to build,
- visualizer tracing was not enabled by either `--enable-wifiviz=1` or a
  script-side `enableWiFiViz = true` default.

### Script mode runs but no viewer appears

Check that:

- `build/WiFiVizApp` exists,
- the script calls `WiFiVizHelper::MaybeEnableVisualizer(..., true)`,
- the environment variable `WIFIVIZ_DISABLE_VIEWER` is not set to `1`,
- you are running from the ns-3 root directory.

### Very large simulations are slow

Use sampled visualization:

```bash
./ns3 run "your-target --enable-wifiviz=1 --precise=0 --rough=10"
```

Increase `rough` to reduce the number of displayed PPDU samples.

## License

Unless otherwise noted, this project is licensed under the MIT License. Some
ns-3-derived example files keep their original GPL-2.0-only SPDX headers. The
license set is GPLv2-compatible for ns-3 app-store contribution purposes.
