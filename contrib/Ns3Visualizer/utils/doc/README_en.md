# WiFi-Visualizer for ns-3

[English](README_en.md) | [中文](README_ch.md) | [Project Home](../../README.md)

WiFi-Visualizer is a plugin-style Qt visualization module for ns-3 Wi-Fi simulations. It supports a full graphical workflow and a one-click workflow for existing ns-3 scripts.

![Overview](../../img/overview.png)

## Run Modes

Full mode:

```bash
./ns3 run visualizer
```

One-click mode:

```bash
./ns3 run "<target> --enable-visualizer=1"
```

If your script sets `enableVisualizer = true` before calling `QNs3Helper::MaybeEnableVisualizer`, the command-line parameter is optional and `./ns3 run <target>` can also open the UI.

## UI Overview

![Configuration Dashboard](../../img/config-dashboard.png)

- Building: dimensions, walls, simulation duration, and environment settings.
- Node AP/STA: PHY/MAC, channel, mobility, antenna, RTS/CTS, aggregation, beacon, and EDCA/QoS settings.
- Flow: source, destination, start/stop time, packet size, and traffic model.
- Script generation: JSON export and automatic ns-3 C++ script generation.

![Result Overview](../../img/result-overview.png)

- PPDU Timeline, Channel-State Timeline, and PHY-State Timeline.
- Throughput, delay, delay CDF, node throughput, frame composition, RX outcome, MCS distribution, and PHY-state pie charts.
- Read-only output viewer for build/run logs.

## More Details

The complete README is maintained at [../../README.md](../../README.md).
