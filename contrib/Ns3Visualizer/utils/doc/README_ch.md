# Ns-3 PPDU Visualizer

[English](README_en.md) | [中文](README_ch.md)

一个基于 **Qt** 的 **ns-3 PHY/MAC 层 PPDU 活动**可视化工具，旨在提供无线网络仿真领域的**交互式、基于时间的 PPDU 级可视化分析**。

本项目专注于 **PPDU 时间线、吞吐折线图、自定义仿真场景** 等能力，支持：
- 节点视角的 PPDU 传输
- 信道视角的 PPDU 传输
- PPDU 传输冲突标注
- 帧类型识别
- 时间范围选择与自动缩放
- 吞吐量 / 信道利用率统计

它是一个**研究与调试工具**，而非完整的网络模拟器或 NetAnim 替代品。

---

## ✨ 功能特性

- 📊 **PPDU 时间线视图**  
  沿时间轴可视化 PPDU 传输，分为 **节点视角** 和 **信道视角**。

- 🧠 **基于通道的重叠布局**  
  自动将重叠的 PPDU 分割到不同通道以避免遮挡，并标注重叠关系。

- 🖱 **丰富的交互功能**
  - **鼠标悬停** 查看 PPDU 详情（帧类型、持续时间、fail-reason、节点 MAC 等）
  - **左键拖动** 平移时间线
  - **鼠标滚轮** 缩放并调整时间轴粒度
  - **右键拖动** 选择时间范围并计算统计指标

- 📐 **时间范围分析**
  对选定时间区间（需启用全量模式）统计：
  - 信道繁忙时间
  - 空闲时间
  - 利用率
  - 吞吐量（Mbps）

- 🪟 **叠加式 UI（NetAnim 风格）**
  半透明浮动叠加层用于：
  - PPDU 信息
  - 图例
  - 统计数据

- 🖼 **保存为图片**  
  将当前时间线视图保存为 PNG/JPG。

---

## 🗂 项目结构

```

|── Qt/
│   └── QT_PRJ/                 # Qt 可视化工程
│       ├── Ns3Visualizer/
│       │   ├── ppdu_timeline_view.*
│       │   ├── ppdu_info_overlay.*
│       │   ├── legend_overlay.*
│       │   └── ...
│       ├── CMakeLists.txt
│       └── ...
├── ns-3.46/                     # ns-3（仅包含 contrib 源码）
└── utils/doc/
```

> ⚠️ **Note**  
> Qt 端与 ns-3 端的相对位置不固定，只需 Qt 端能够访问到 ns-3 路径即可。

---

## 🔧 依赖

- **Qt 6**（Qt 5 可通过小改动兼容）
- **C++17** 或更新标准
- **ns-3.46** 或更高版本
- Linux 环境

> ⚠️ **Note**  
> `ns-3.46` 的完整源码**不包含在此仓库**，仅提供 contrib 模块源码。如需完整 ns-3，请前往 [ns-3 官网](https://www.nsnam.org/) 下载。

---

## 🚀 快速运行（预编译包）

- **AppImage**（桌面环境）  
  直接运行 Ns3Visualizer.AppImage 即可打开 UI。

- **无桌面环境可执行包**  

```bash
# 无需修改 Qt 源码
tar xJvf Ns3Visualizer-user.tar.xz
cd Ns3Visualizer-user
chmod +x run.sh
./run.sh
```

```bash
# 需要修改 Qt 源码
tar xJvf Ns3Visualizer-dev.tar.xz
cd Ns3Visualizer-dev
chmod +x run.sh
./run.sh
```

---

## 🛠 从源码构建（Qt 端）

```bash
cd Visualization/Qt
mkdir build && cd build
cmake ..
make
./Ns3Visualizer
```

> Qt 应用只负责**可视化展示**，不会编译或运行 ns-3。

---

## 📥 数据输入

PPDU 数据通常来自 ns-3 的 trace hook 或自定义日志输出（JSON/CSV）。
每条 PPDU 记录通常包含：
- 节点或 AP 标识
- 起止时间（ns）
- 帧类型
- 聚合 MPDU 数量
- Payload 大小

---



## 🧪 项目状态

- ✔ PPDU 时间线与叠加 UI
- ✔ 时间范围统计
- ✔ 交互操作
- ⏳ 复杂流量配置
- ⏳ MLO

---

## 🎯 设计目标

- 聚焦 PHY/MAC 行为，而非拓扑动画
- 纳秒级时间对齐
- 便于研究与扩展

---

## 📜 许可证

本项目采用 **MIT License**。

---

## 🙋 作者

- **张恺 u202414527@hust.edu.cn**   
- **宓呈祥 michengxiang@hust.edu.cn**
  

---

## ⭐ 致谢

- ns-3 Simulator
- NetAnim（UI 风格灵感）
- Qt Framework

> 如果你在研究、课程或工具中使用了该项目，欢迎在论文或报告中致谢。