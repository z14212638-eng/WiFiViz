# QoS Mixed Traffic

A Wi-Fi 802.11ax scenario with one AP and three STAs. The script enables QoS MAC
support and runs three UDP flows with different packet sizes and intervals so
the timeline shows multiple traffic patterns in one BSS.

Useful command:

```bash
./ns3 run "qos-mixed-traffic --enable-wifiviz=1 --precise=1"
```

For large timeline tests:

```bash
./ns3 run "qos-mixed-traffic --enable-wifiviz=1 --precise=0 --rough=5"
```
