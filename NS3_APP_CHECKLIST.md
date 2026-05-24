# ns-3 App Store Contribution Checklist

This repository is prepared as a standalone ns-3 contrib module. A clone or
release archive should unpack directly to the `WiFiViz/` module
directory, and users should install it as:

```text
/path/to/ns-3.46/contrib/WiFiViz
```

## Packaging

- Repository layout: standalone module directory, not a full ns-3 source tree.
- Module root: `CMakeLists.txt`, `model/`, `helper/`, `examples/`, `test/`,
  `doc/`, `ui/`, `Simulation/`, and `tools/`.
- Scratch launcher: `tools/visualizer.cc` is inside the module and may be
  copied to `/path/to/ns-3.46/scratch/visualizer.cc` for `./ns3 run visualizer`.
- Generated data: `Simulation/Designed/`, local `scratch/`, build directories,
  and local tool metadata are ignored.

## ns-3 Checklist Status

| Item | Status |
| --- | --- |
| Can be downloaded into `contrib/` | Yes. Clone as `contrib/WiFiViz`. |
| Builds in debug and optimized modes | Intended for ns-3.46; verify before each release. |
| Sphinx documentation in `doc/` | Present: `doc/wifiviz.rst`. |
| Examples | Present: `examples/wifiviz-basic-example.cc` and default GUI scenarios under `Simulation/Default/`. |
| Regression tests | Present: `test/wifiviz-test-suite.cc`; currently covers helper-level behavior. |
| Coding style | Module follows ns-3 CMake contrib structure; run style review before release. |
| GPLv2-compatible license | Yes. MIT by default, with ns-3-derived examples retaining GPL-2.0-only SPDX headers. |
| Release numbering plan | Use semantic versioning, starting with `v0.1.0` for the first app-store candidate. |
| App name | `WiFiViz` / `WiFiViz`. |
| Thumbnail icon | Not yet finalized; add an app-store thumbnail before publishing. |
| App-store tab content | README provides draft overview, install, usage, UI, troubleshooting, and license text. |
| Requires ns-3 core patches | No. Full GUI mode uses an optional scratch launcher instead of modifying ns-3 core files. |

## Release Notes Template

Use this for future app-store releases:

```text
Version: vX.Y.Z
ns-3 compatibility: ns-3.46 or later compatible release
Archive layout: WiFiViz/ standalone contrib module
Required external dependencies: Qt 6 or Qt 5.15+, Boost Interprocess
Known limitations:
- Full GUI command requires copying tools/visualizer.cc to scratch/visualizer.cc.
- Regression tests are currently minimal.
```
