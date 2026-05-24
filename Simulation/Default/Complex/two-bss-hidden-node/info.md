# Two BSS Hidden Node

Two AP/STA pairs share the same channel. The APs are separated, and each STA is
near its own AP. Both downlink flows run at the same time, which creates a
denser PPDU timeline than the simple examples.

Useful command:

```bash
./ns3 run "two-bss-hidden-node --enable-wifiviz=1 --precise=1"
```
