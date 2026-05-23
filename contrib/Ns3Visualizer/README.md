# Ns3Visualizer contrib module

This directory is the ns-3 contrib module for `ns3-Visualizer`.  It contains the ns-3 tracing components, helper APIs, Qt frontend, script generator, examples, and default GUI scenarios.

For installation, build instructions, screenshots, and GIF placeholders, see the repository-level [README](../../README.md).

## Main components

- `model/` and `helper/`: PPDU/MAC/PHY trace collection and helper APIs.
- `ui/`: Qt-based configuration and visualization frontend.
- `ui/utils/ns3-script-generator.cc`: standalone script generator used by the GUI.
- `Simulation/Default/`: built-in default scenarios shown in the GUI.
- `examples/`: ns-3 examples using the visualizer helper.

## Expected location

Place this directory as:

```text
ns-3.46/contrib/Ns3Visualizer
```

Then run:

```bash
cd /path/to/ns-3.46
./ns3 configure
./ns3 build
```
