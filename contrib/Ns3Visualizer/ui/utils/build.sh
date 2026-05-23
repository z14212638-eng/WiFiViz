#!/bin/bash

# 编译 NS3 脚本生成器

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"

NS3_ROOT="${1:-${NS3_ROOT:-$(realpath "$SCRIPT_DIR/../../../..")}}"
if [ -z "$NS3_ROOT" ]; then
    echo ""
    echo "========================================"
    echo "✗ NS3_ROOT 未设置"
    echo "========================================"
    echo "用法: $0 <ns3_root_path>"
    echo "或设置环境变量: NS3_ROOT=/path/to/ns-3"
    exit 1
fi

if [ ! -d "$NS3_ROOT" ]; then
    echo ""
    echo "========================================"
    echo "✗ NS3_ROOT 目录不存在: $NS3_ROOT"
    echo "========================================"
    exit 1
fi

echo "========================================"
echo "编译 NS3 脚本生成器"
echo "========================================"

# 创建 build 目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 运行 cmake
echo "运行 CMake..."
cmake -DNS3_ROOT="$NS3_ROOT" ..

# 编译
echo "编译中..."
make

# 检查是否成功
if [ -f "$BUILD_DIR/ns3-script-generator" ]; then
    echo ""
    echo "========================================"
    echo "✓ 编译成功！"
    echo "========================================"
    echo "可执行文件位置: $BUILD_DIR/ns3-script-generator"
    echo ""
    echo "使用方法:"
    echo "  $BUILD_DIR/ns3-script-generator <项目路径> [输出脚本路径]"
    echo ""
    echo "示例:"
    echo "  $BUILD_DIR/ns3-script-generator \\"
    echo "    ../ns-3.46/contrib/Ns3Visualizer/Simulation/Designed/MySimulationProject \\"
    echo "    ../ns-3.46/scratch/generated-simulation.cc"
else
    echo ""
    echo "========================================"
    echo "✗ 编译失败"
    echo "========================================"
    exit 1
fi

