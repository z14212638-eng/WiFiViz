# Three BSS Overlap

Three overlapping BSSs share the same 5 GHz channel. Each AP advertises a
separate SSID and serves two associated STAs. Every AP sends saturated downlink
UDP traffic to both of its STAs, so the scenario stresses inter-BSS contention,
CCA busy time, queueing delay, and per-BSS fairness.

Useful command:

```bash
./ns3 run "three-bss-overlap --enable-wifiviz=1 --precise=1"
```

For heavier offered load:

```bash
./ns3 run "three-bss-overlap --enable-wifiviz=1 --precise=1 --per-flow-rate=150Mbps"
```
