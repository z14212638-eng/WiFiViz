# WiFi-Visualizer for ns-3

[English](README_en.md) | [中文](README_ch.md) | [项目首页](../../README.md)

WiFi-Visualizer 是一个面向 ns-3 Wi-Fi 仿真的插件式 Qt 可视化模块。它提供两种使用方式：用于图形化搭建、生成、运行和分析仿真的全量模式，以及用于给已有 ns-3 脚本快速接入可视化的一键模式。

本仓库只发布 `contrib/Ns3Visualizer`，不包含 ns-3 完整源码，也不要求对 ns-3 核心源码进行破坏性修改。

![Overview](../../img/overview.png)

## 主要功能

- 全量模式仿真配置界面，可通过图形化方式构建 Wi-Fi 场景。
- 一键模式可在已有 ns-3 脚本中通过 `QNs3Helper::MaybeEnableVisualizer` 接入可视化。
- PPDU Timeline、Channel-State Timeline、PHY-State Timeline，用于微观 PHY/MAC 行为分析。
- 吞吐、时延、时延 CDF、节点吞吐、帧组成、接收结果、MCS 分布、PHY 状态扇形图等统计图。
- 支持缩放、平移、时间区间选择、详情面板、只读输出窗口和图片导出。
- 使用 JSON 作为中间配置格式，并通过 `ns3-script-generator` 自动生成 ns-3 C++ 脚本。

## 仓库结构

```text
contrib/Ns3Visualizer/
├── CMakeLists.txt
├── README.md
├── doc/                         # ns-3 模块文档
├── examples/                    # 接入可视化 helper 的示例脚本
├── helper/                      # QNs3Helper 和可视化接入逻辑
├── model/                       # trace 采集、共享内存记录和工具
├── Simulation/
│   └── Default/                 # GUI 中展示的内置示例场景
├── test/                        # ns-3 测试骨架
├── ui/                          # Qt 应用和 ns3-script-generator
└── utils/doc/                   # 中英文 README
```

`Simulation/Designed/` 下的生成实验、`scratch/` 脚本、构建目录、`.codex`、以及同级的 `contrib/nr` 模块都不属于本工具的必要提交内容。

## 环境要求

- ns-3.46 或兼容的新版本。
- Qt 5.15+ 或 Qt 6。
- CMake 3.16+。
- 支持 C++17 的编译器。
- 主要测试环境为 Linux。

## 安装

将本仓库放到 ns-3 源码树的 `contrib` 目录下：

```bash
cd /path/to/ns-3.46/contrib
git clone https://github.com/z14212638-eng/Ns3-based-Visualization.git Ns3Visualizer
cd ..
./ns3 configure
./ns3 build
```

构建后会生成 Qt 应用 `build/Ns3VisualizerApp` 和脚本生成器 `build/ns3-script-generator`。

## 全量模式

全量模式的标准启动方式为：

```bash
./ns3 run visualizer
```

为了在不修改 ns-3 核心源码的前提下支持这个命令，需要把 launcher 脚本放入 `scratch/`：

```bash
cp contrib/Ns3Visualizer/tools/visualizer.cc scratch/visualizer.cc
```

如果你的仓库中暂时没有 `contrib/Ns3Visualizer/tools/visualizer.cc`，可以使用 [对开发者的建议](#对开发者的建议) 中给出的 launcher 内容创建 `scratch/visualizer.cc`。这样做是有意设计：ns-3 只能运行它已知的 target，如果要直接内置 `visualizer` target，就需要修改 ns-3 的构建或源码文件；而本项目原则上不破坏 ns-3 源码，只作为插件式可视化器存在。

![Full Mode](../../img/full-mode.gif)

全量模式流程：

1. 使用 `./ns3 run visualizer` 打开可视化器。
2. 配置建筑、节点、业务流、PHY/MAC 参数和仿真选项。
3. 点击 `Generate` 生成 JSON 配置和独立 ns-3 C++ 脚本。
4. 工具自动 build/run 生成的 target，并跳转到结果展示界面。
5. 点击输出按钮可以打开只读输出窗口，查看 ns-3 build/run 日志。

## 一键模式

一键模式用于已有 ns-3 脚本。默认命令为：

```bash
./ns3 run "<target> --enable-visualizer=1"
```

示例：

```bash
./ns3 run "QNs3-example --enable-visualizer=1"
./ns3 run "wifi-txop-aggregation --enable-visualizer=1 --precise=1 --rough=1"
```

脚本中需要暴露并使用可视化开关：

```cpp
bool enableVisualizer = false;
bool precise = true;
uint32_t rough = 1;

CommandLine cmd(__FILE__);
cmd.AddValue("enable-visualizer", "Enable WiFi-Visualizer timeline capture", enableVisualizer);
cmd.AddValue("precise", "Capture every PPDU without down-sampling", precise);
cmd.AddValue("rough", "Down-sample PPDU timeline to 1/N", rough);
cmd.Parse(argc, argv);

Ptr<SniffUtils> visualizer = QNs3Helper::MaybeEnableVisualizer(
    enableVisualizer,
    allDevices,
    simulationTimeSeconds,
    precise);
```

如果你在脚本中直接把 `enableVisualizer` 设置为 `true`，并在之后调用 `MaybeEnableVisualizer`，那么命令行里不写 `--enable-visualizer=1` 也可以：

```bash
./ns3 run <target>
```

但对于示例脚本和可复现实验，仍然建议保留命令行参数，这样用户不用改源码就能切换是否启用可视化。

![One-Click Mode](../../img/one-click-mode.gif)

## UI 模块说明

### 启动页

启动页用于选择全量模式或一键模式，也会展示内置示例场景，让用户可以快速进入默认仿真。

![Start Page](../../img/start-page.png)

### 仿真配置界面

配置界面用于全量模式。下图只展示了部分配置画面，实际配置项更多。

![Configuration Dashboard](../../img/config-dashboard.png)

主要配置区域：

- Building：建筑尺寸、墙体设置、房间布局、仿真时长和环境级参数。
- Layout Canvas：AP/STA 添加、删除、拖拽、位置同步、地图放大和空间分布检查。
- Node AP/STA：无线标准、信道、频率、发射功率、天线、移动模型、RTS/CTS、聚合、Beacon、EDCA/QoS 等 PHY/MAC 参数。
- Flow：源节点、目的节点、UDP 类业务、Bulk Transfer 类业务、开始/结束时间、包大小、负载和速率参数。
- Script Generation：导出 JSON 配置，并通过 `ns3-script-generator` 生成 ns-3 C++ 脚本。

### 结果展示界面

结果展示界面集成 timeline、统计图、详情面板和输出查看。

![Result Overview](../../img/result-overview.png)

Timeline：

- PPDU Timeline：将每个 PPDU 显示为带持续时间的时间段对象，并通过 lane 分离重叠传输。
- Channel-State Timeline：根据 PPDU 起止时间重构 IDLE、BUSY、COLLISION 信道状态。
- PHY-State Timeline：展示 IDLE、TX、RX、CCA_BUSY、SWITCHING、SLEEP、OFF 等 PHY 状态。

统计图：

- Throughput Chart：吞吐随时间变化和平均吞吐。
- Delay Charts：队列时延和 MAC 端到端时延，并支持 CDF View。
- Frame Composition Chart：Data、Control、Management 等帧类型占比。
- Node Throughput Chart：节点级吞吐贡献。
- RX-Outcome Chart：成功接收、冲突和解码失败统计。
- MCS Distribution Chart：MCS 使用频率。
- PHY State Pie Chart：PHY 状态时长占比，并显示单 PHY 平均时长。

### 输出窗口

输出窗口以只读工业灰面板展示 build/run 日志，不直接占用主结果区域。

![Output Viewer](../../img/output-viewer.png)

## 数据流

```text
GUI 配置
      ↓
Simulation/Designed/<scenario>/ 下的 JSON 文件
      ↓
ns3-script-generator
      ↓
scratch/<generated-target>.cc
      ↓
ns-3 build/run
      ↓
共享内存 trace records
      ↓
Qt 结果可视化界面
```

一键模式会跳过 GUI 配置和脚本生成阶段，直接在用户脚本中接入 trace 采集。

## 对开发者的建议

推荐的 `scratch/visualizer.cc` launcher：

```cpp
#include "ns3/core-module.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using namespace ns3;

int main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    namespace fs = std::filesystem;
    fs::path cwd = fs::current_path();
    fs::path appPath = cwd / "build" / "Ns3VisualizerApp";
    if (!fs::exists(appPath))
    {
        appPath = cwd / "Ns3VisualizerApp";
    }

    const int ret = std::system(appPath.string().c_str());
    if (ret != 0)
    {
        std::cerr << "Failed to launch visualizer app: " << appPath << std::endl;
        return 1;
    }
    return 0;
}
```

开发注意事项：

- 保持 ns-3 核心源码不变，优先使用 scratch launcher、contrib helper 和生成脚本。
- 除非是有意提供示例，不要提交 `Simulation/Designed/*` 生成实验。
- 不要提交本地 `.codex`、构建目录、`scratch/`、二进制发布包或同级 `contrib/nr` 模块。
- 新增 Qt 源文件时，需要同步维护 `ui/CMakeLists.txt` 和 `ui/Ns3Visualizer.pro`。
- 新增统计图时，应在 UI 和 README 中说明统计口径与单位。

## 许可证

本项目采用 MIT License。

## 作者

- 张恺，u202414527@hust.edu.cn
- 宓呈祥，michengxiang@hust.edu.cn

## 致谢

- ns-3 Simulator
- Qt Framework
- NetAnim UI 风格参考
