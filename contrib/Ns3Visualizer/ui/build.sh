#!/bin/bash

# CMake 构建脚本
# 使用方法: ./build.sh [Debug|Release]

set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build-cmake"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR_PATH="$SOURCE_DIR/$BUILD_DIR"

echo "================================"
echo "Ns3Visualizer CMake 构建脚本"
echo "================================"
echo "构建类型: $BUILD_TYPE"
echo "源目录: $SOURCE_DIR"
echo "构建目录: $BUILD_DIR_PATH"
echo ""

# 创建并进入构建目录
mkdir -p "$BUILD_DIR_PATH"
cd "$BUILD_DIR_PATH"

# 运行 CMake 配置
echo "配置项目..."
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..

# 编译
echo ""
echo "编译项目..."
cmake --build . --config "$BUILD_TYPE" -j$(nproc)

echo ""
echo "================================"
echo "构建完成！"
echo "可执行文件位置: $BUILD_DIR_PATH/Ns3Visualizer"
echo "================================"
echo ""
if [ -f "$BUILD_DIR_PATH/Ns3Visualizer" ]; then
      cp -f "$BUILD_DIR_PATH/Ns3Visualizer" "$SOURCE_DIR/Ns3Visualizer"
      echo "已复制可执行文件到当前目录: $SOURCE_DIR/Ns3Visualizer"
      echo ""
else
      echo "未找到可执行文件，跳过复制: $BUILD_DIR_PATH/Ns3Visualizer"
      echo ""
fi
echo "运行程序:"
echo "  cd $BUILD_DIR_PATH && ./Ns3Visualizer"
