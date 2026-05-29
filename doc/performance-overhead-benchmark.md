# WiFiViz Performance Overhead Benchmark

This benchmark measures the runtime overhead introduced by the WiFiViz data
collection module under the same ns-3 Wi-Fi workload.

## Experimental Design

Use a paired A/B experiment:

- Baseline: run the same ns-3 simulation without enabling WiFiViz.
- WiFiViz: run the same ns-3 simulation with `WiFiVizHelper::MaybeEnableVisualizer`
  enabled, but without launching the Qt viewer.

The benchmark scenario is `wifiviz-overhead-benchmark`. It creates one AP and
multiple STA nodes. The AP sends UDP traffic to each STA over 802.11n with a
constant PHY rate. This produces frequent PPDU transmissions, PHY state changes,
queueing events, and ACK events, which are the main sources of WiFiViz trace
collection overhead.

The two variants keep the same topology, traffic parameters, random seed, and
simulation duration. The batch runner executes the baseline and WiFiViz variants
as adjacent pairs to reduce bias from machine load, cache state, and temperature.

## Metrics

Primary metric:

- `simulator_run_ms`: wall-clock time spent inside `Simulator::Run()`. This is
  the main metric to report because it captures the real slowdown observed by an
  ns-3 user during event execution.

Secondary metrics:

- `total_ms`: full process time measured by the benchmark program, including
  scenario setup, WiFiViz initialization, `Simulator::Run()`, and
  `Simulator::Destroy()`.
- `viz_init_ms`: time spent enabling WiFiViz before the simulation starts. This
  includes shared memory removal/creation and trace-source registration.
- `received_packets`: sanity-check metric. The baseline and WiFiViz runs should
  receive the same number of application packets.

The recommended overhead formula is:

```text
Overhead (%) = (T_wifiviz - T_baseline) / T_baseline * 100
```

Use `simulator_run_ms` as `T` for the main result. Report `total_ms` separately
if the paper discusses end-to-end execution time including initialization.

## Why Not Only Measure Internal Callback Time

Only timing the data collection callbacks is not sufficient as the main result.
It misses effects that matter to the actual simulator runtime, such as:

- extra events scheduled by WiFiViz for PPDU finalization;
- trace-source dispatch cost;
- shared-memory locking and memory writes;
- container lookup and allocation effects;
- changes in CPU cache behavior during the simulation.

Internal callback timing can be useful as a diagnostic metric, but the paper
should use the paired wall-clock runtime difference as the primary overhead
measurement.

## How to Run

Configure ns-3 with examples enabled and build the benchmark:

```bash
cd /path/to/ns-3.46
./ns3 configure --enable-examples --build-profile=default
./ns3 build wifiviz-overhead-benchmark
```

Run a full measurement:

```bash
for i in $(seq 1 10); do
  ./ns3 run "wifiviz-overhead-benchmark --enable-wifiviz=0 --simulation-time=20 --n-sta=8 --interval-us=1000"
  ./ns3 run "wifiviz-overhead-benchmark --enable-wifiviz=1 --launch-viewer=0 --simulation-time=20 --n-sta=8 --interval-us=1000"
done
```

Capture the benchmark output from each paired run and compute the overhead from
the reported timing fields. Keep generated CSV files, plots, and paper figures
outside the repository or under an ignored local `results/` directory.

For a heavier stress test, increase `--n-sta`, decrease `--interval-us`, or
increase `--simulation-time`.

## Suggested Paper Text

The runtime overhead of WiFiViz was evaluated using a paired A/B experiment.
For each repetition, the same ns-3 Wi-Fi scenario was executed twice: once as a
baseline without WiFiViz and once with WiFiViz data collection enabled. The Qt
viewer was disabled in both cases, so the measured overhead corresponds to the
simulation-side collection path only. The scenario used one AP and multiple STA
nodes with saturated UDP downlink traffic, which continuously triggers PPDU,
PHY-state, queueing-delay, and ACK-related trace callbacks.

The primary metric was wall-clock time spent in `Simulator::Run()`, because this
measures the additional cost paid during discrete-event execution. End-to-end
process time and WiFiViz initialization time were also recorded as secondary
metrics. For each metric, the relative overhead was computed as
`(T_wifiviz - T_baseline) / T_baseline * 100%`.
