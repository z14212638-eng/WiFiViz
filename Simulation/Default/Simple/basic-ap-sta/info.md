# Basic AP/STA

A minimal WiFiViz-ready Wi-Fi scenario with one AP, one STA, and a UDP downlink
flow from the AP to the STA.

The script demonstrates the recommended simple structure:

- include `ns3/wifiviz.h`
- build AP and STA `NetDeviceContainer`s
- merge them with `WiFiVizHelper::MergeDevices`
- pass `enableWiFiViz`, devices, simulation time, and viewer behavior into
  `WiFiVizHelper::MaybeEnableVisualizer`

Useful command:

```bash
./ns3 run "basic-ap-sta --enable-wifiviz=1 --launch-viewer=1"
```
