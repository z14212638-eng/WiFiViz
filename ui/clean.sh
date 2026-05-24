#!/bin/bash

# 清理脚本 - 删除所有构建产物

set -e

echo "清理所有构建产物..."

# 删除 CMake 构建目录
rm -rf build-cmake/
rm -rf cmake-build-debug/
rm -rf cmake-build-release/

# 删除旧的 qmake 构建目录
rm -rf build/

# 删除生成的文件
find . -maxdepth 1 -type f \( \
    -name "Makefile*" \
    -o -name "moc_*.cpp" \
    -o -name "ui_*.h" \
    -o -name "qrc_*.cpp" \
    -o -name "*.o" \
    -o -name ".qmake.stash" \
\) -delete

echo "清理完成！"
