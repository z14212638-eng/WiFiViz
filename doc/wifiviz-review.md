# WiFiViz Module Review

Before it can be broadly compatible and robust for an
ns-3-allinone distribution, a handful of build-robustness and portability issues
need to be addressed. The accompanying `git diff` patch fixes the issues in
Part 1. Part 2 lists two smaller items left for you to complete.

The review targeted distribution on multiple platforms (Linux and macOS) and on
hosts that may be missing optional prerequisites (nlohmann/json, Boost, Qt). The
central theme is that the module previously declared none of its third-party
dependencies to CMake and had no configure-time guards, so it always reported
itself as "buildable" and then failed at compile time on any host missing a
dependency.

---

## Part 1: Issues addressed in the patch

### 1. The module did not handle missing prerequisites gracefully

**Problem.** The ns-3 library itself (not just the optional generator) has two
undeclared third-party dependencies:

- `model/QNs3.cc` includes `nlohmann/json.hpp`.
- `model/SniffUtils.{h,cc}` include five `boost/interprocess/...` headers.

Yet the top-level `build_lib()` listed only ns-3 libraries in
`LIBRARIES_TO_LINK` and added no include path or link for either dependency. The
library compiled only by accident when those headers happened to be on the
default system include path. On a host without nlohmann/json (and ns-3 does not
bundle it), the build fails with:

```
model/... fatal error: 'nlohmann/json.hpp' file not found
```

Worse, `./ns3 configure` still listed `wifiviz` under "Modules configured to be
built", so users saw a green configure and then a broken build. The
`ui/utils/CMakeLists.txt` "header-only fallback" did not actually degrade
gracefully either: when `nlohmann_json` was not found it printed "using
header-only version" and continued, but only added `/usr/include`,
`/usr/local/include`, and `model/` to the include path (none of which contain
the header), merely deferring the same fatal error.

**Motivation.** For an allinone distribution shipped to users who may not have
these libraries, a missing optional dependency must not break the build. The
ns-3 convention (used by `brite`, `click`, `openflow`, etc.) is to detect the
prerequisite at configure time and skip the module cleanly so it appears under
"Modules that cannot be built".

**Fix (in `CMakeLists.txt` and `ui/utils/CMakeLists.txt`).**

- Added a configure-time guard: `find_package(nlohmann_json 3.2.0)` and
  `check_include_file_cxx(boost/interprocess/managed_shared_memory.hpp ...)`.
  If either is missing, the module emits a `STATUS` message and `return()`s, so
  it is reported as not-buildable instead of failing at compile time.
- Added `nlohmann_json::nlohmann_json` to the library's `LIBRARIES_TO_LINK`, and
  propagated the Boost include directory to the library target.
- Replaced the broken "header-only fallback" in `ui/utils/CMakeLists.txt` with
  `find_package(nlohmann_json 3.2.0 REQUIRED)` (the parent guard already
  guarantees it is present by the time this subdirectory is reached).

### 2. The module forced C++23, which is unnecessary and breaks older toolchains

**Problem.** The top-level `CMakeLists.txt` set:

```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

Most of ns-3 builds as C++20. With `STANDARD_REQUIRED ON`, any compiler that
does not fully advertise C++23 is a hard configure/compile failure rather than a
fallback to C++20 -- a real portability hazard for older GCC/Clang and for the
AppleClang shipped on many macOS machines. A scan of the library, helper, and
tools sources found no C++23-specific features in use, so the requirement was
gratuitous.

(Note: this did not leak into other modules. Each contrib module is processed in
its own CMake variable scope via `add_subdirectory`, so the setting affected only
wifiviz's own targets. The risk was a self-inflicted hard-fail, not corruption of
other modules.)

**Motivation.** The toolchain bar should match the rest of ns-3 so the module
builds anywhere ns-3 builds.

**Fix (in `CMakeLists.txt`).** Removed the forced C++23 setting; the library now
inherits ns-3's C++ standard (C++20 as of the ns-3.48 release).

### 3. macOS configure warnings and a latent macOS path bug (MACOSX_BUNDLE)

**Problem.** Configuring on macOS produced eight CMake developer warnings about
uninitialized `MACOSX_BUNDLE_*` variables, originating from
`set_target_properties(WiFiVizApp PROPERTIES ... MACOSX_BUNDLE TRUE)` in
`ui/CMakeLists.txt`. The warnings themselves are cosmetic, but `MACOSX_BUNDLE
TRUE` also has a real consequence: on macOS it places the binary at
`WiFiVizApp.app/Contents/MacOS/WiFiVizApp`, whereas the launcher and the README
expect a plain executable at `build/WiFiVizApp`. So the GUI launch path would be
broken on macOS regardless of the warnings.

**Motivation.** A clean configure on all supported platforms, and a consistent
binary location that the launcher and docs can rely on.

**Fix (in `ui/CMakeLists.txt`).** Removed `MACOSX_BUNDLE TRUE`. This silences the
eight developer warnings and makes the executable land at `build/WiFiVizApp` on
macOS as well, matching the launcher and README.

### 4. The 1 GiB shared-memory segment was over-provisioned and could abort the run

**Problem.** `model/SniffUtils.cc` allocated a 1 GiB Boost.Interprocess
`managed_shared_memory` segment:

```cpp
m_shm = new boost::interprocess::managed_shared_memory(open_or_create, SHM_NAME,
                                                       1024UL * 1024 * 1024); // 1GB
```

Analysis of the data structures shows the segment holds exactly one fixed-size
`RingBuffer` object: two indices, a mutex, a condition variable, and
`PPDU_Meta records[MAX_PPDU_NUM]` with `MAX_PPDU_NUM = 1 << 20`. With
`sizeof(PPDU_Meta) == 168` bytes, the records array is ~168 MiB and the whole
`RingBuffer` is ~168 MiB. The 1 GiB request was therefore roughly 6x larger than
needed, and it bought no extra capacity: `AppendPpdu()` wraps with
`write_index % MAX_PPDU_NUM` and never consults `read_index`, so capacity is
governed entirely by `MAX_PPDU_NUM`, independent of segment size.

The over-sizing also created a real failure mode. If the OS cannot back the
requested segment, the `managed_shared_memory` constructor throws
`boost::interprocess::interprocess_exception`. `Initialize()` had no
`try/catch`, and its caller `QNs3Helper::EnableVisualizer()` ignored the `bool`
return value, so the exception propagated out of the simulation and aborted the
entire `./ns3 run` (uncaught exception -> `std::terminate`). This bites in three
realistic situations:

- macOS, where POSIX shared memory (`shm_open` + `ftruncate` to 1 GiB) is far
  more constrained than Linux;
- CI/containers, where the default `/dev/shm` is 64 MiB;
- low-RAM Linux hosts, where `/dev/shm` defaults to half of RAM.

On a typical Linux dev box it "worked" only because tmpfs backing is sparse and
lazily committed.

**Motivation.** The segment should be sized to what it actually needs, should
self-tune if the data structures change, and a failure to obtain it should
disable visualization rather than kill the user's simulation.

**Fix (in `model/SniffUtils.cc` and `helper/QNs3-helper.cc`).**

- Replaced both `1024*1024*1024` literals with `sizeof(RingBuffer) + 64 KiB`
  (the small slack covers the Boost segment-manager bookkeeping). This
  auto-tunes if `MAX_PPDU_NUM` or `PPDU_Meta` change. To increase PPDU history,
  the knob to turn is now `MAX_PPDU_NUM`, and the segment size follows
  automatically.
- Wrapped the allocation in `try/catch`. On failure it prints a clear message,
  returns `false`, and `EnableVisualizer()` now honors that return and disables
  visualization, so the simulation continues normally instead of aborting.

The Qt reader opens the segment with `open_only` and no size
(`qt_ppdu_reader.cpp`), so it inherits whatever size the writer creates; this
change required no reader-side modification.

### 5. The launcher required a manual copy into scratch/ (unnecessary)

**Problem.** The README instructed users to copy `tools/visualizer.cc` into
`scratch/` so that `./ns3 run visualizer` would work, with the stated rationale
that supporting the command otherwise "would require changing ns-3's own source
tree." That rationale is not correct: `./ns3 run <name>` resolves registered
example/program targets across `src/` and `contrib/` modules, not just
`scratch/`. The supported, no-core-changes mechanism is `build_lib_example()` in
the module's `examples/CMakeLists.txt`. In addition, `tools/visualizer.cc` was
referenced by no CMakeLists at all, so it was effectively dead unless manually
copied. (`tools/visualizer.cc` is a ~30-line launcher that `std::system()`-execs
`build/WiFiVizApp`; it only needs `core-module` for `CommandLine`.)

**Motivation.** Remove a fragile manual step and make the launcher a
first-class, automatically built target.

**Fix (in `examples/CMakeLists.txt`).** Registered the launcher as the module
example `wifiviz-visualizer`, so `./ns3 run wifiviz-visualizer` works directly
with no copy step. The README was updated to match (the copy step and the
incorrect rationale were removed; Run Mode 3 now uses
`./ns3 run wifiviz-visualizer`). Note that example targets build under
`--enable-examples`; the README's Build section now reflects this.

### 6. Qt GUI was a hard dependency that could break the whole tree

**Problem.** `add_subdirectory(ui)` was unconditional, and `ui/CMakeLists.txt`
used `find_package(Qt5 5.15 REQUIRED ...)` as the fallback. On a host with no Qt
at all, this made the entire `./ns3 configure` fail -- not just wifiviz -- so one
optional GUI module could break configuration of the whole tree.

**Motivation.** The GUI front-end is optional; its absence should skip only the
front-end, never the rest of ns-3.

**Fix (in `CMakeLists.txt`).** Gated `add_subdirectory(ui)` behind a Qt probe
(`find_package(Qt6 ... QUIET)` then `find_package(Qt5 5.15 ... QUIET)`). When Qt
is not found, the ns-3 library still builds and a `STATUS` message reports that
the GUI front-end is skipped.

### 7. Unconditional stdc++fs link broke Clang/libc++ (macOS)

**Problem.** `ui/utils/CMakeLists.txt` linked `stdc++fs` unconditionally. That
library is a GCC/libstdc++ artifact (needed only before GCC 9 for `<filesystem>`)
and does not exist on Clang/libc++, so the link step would fail on macOS.

**Motivation.** Portability of the generator target across compilers.

**Fix (in `ui/utils/CMakeLists.txt`).** Link `stdc++fs` only for
`CMAKE_CXX_COMPILER_ID STREQUAL "GNU"` with version `< 9.0`.

---

## Part 2: Remaining work for the author

These two items are not in the patch and are left for you to complete.

### A. Remove leftover development artifacts

The package contains build artifacts that should not ship in an App Store
contrib module, since CMake is the single source of truth for the build:

- `ui/WiFiViz.pro` (qmake project file)
- `ui/build.sh`
- `ui/clean.sh`

Removing these avoids confusion about which build system is authoritative.
(There are also per-directory `.gitignore` files; please confirm they are
intended for the distributed package.)

### B. Add a minimum-version check to find_package(Boost ...)

The patch currently locates Boost.Interprocess headers via
`check_include_file_cxx(...)` and exposes the include path, but does not pin a
minimum Boost version. If the code relies on behavior from a specific
Boost.Interprocess version, please add an explicit minimum, for example:

```cmake
find_package(Boost 1.71 REQUIRED)   # adjust to the minimum you actually require
```

and include that minimum in the README's Requirements section. This makes the
dependency explicit and gives users a clear error if their Boost is too old.

---

## Validation note

The patch could not be compile-validated on the reviewing host because it lacks
nlohmann/json, Boost, and Qt. The CMake changes are idiomatic but should be
validated on a host that has all three installed. One item to confirm there:
that `${libwifiviz}` resolves to the build_lib target name in your ns-3 version
(the standard convention), which the Boost-include line relies on.
