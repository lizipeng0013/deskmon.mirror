#!/bin/bash
# DeskMon 快速启动脚本
# 用法：./start.sh [build|run|clean]
#   build - 重新编译后启动（默认）
#   run   - 跳过编译直接启动
#   clean - 清理 build 目录后重新编译并启动

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN="/home/kookboy/jm-prefix/usr"
LIB_PATH="$TOOLCHAIN/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LIB_PATH"

ACTION="${1:-build}"

# 检查工具链
if [ ! -d "$TOOLCHAIN" ]; then
    echo "错误：找不到 DTK6/Qt6 工具链 $TOOLCHAIN"
    exit 1
fi

# 已有实例则先关闭，避免单实例冲突
pkill -x deskmon 2>/dev/null

case "$ACTION" in
clean)
    echo "清理 build 目录..."
    rm -rf "$SCRIPT_DIR/build"
    ;&  # fallthrough 到 build
build)
    if [ ! -f "$SCRIPT_DIR/build/deskmon" ] || [ "$ACTION" = "clean" ]; then
        echo "编译 DeskMon..."
        mkdir -p "$SCRIPT_DIR/build"
        cd "$SCRIPT_DIR/build" || exit 1
        cmake -DCMAKE_PREFIX_PATH="$TOOLCHAIN" .. || { echo "cmake 失败"; exit 1; }
        make -j"$(nproc)" || { echo "编译失败"; exit 1; }
    else
        # 增量编译（有改动时才重新链接）
        echo "增量编译..."
        cd "$SCRIPT_DIR/build" || exit 1
        make -j"$(nproc)" 2>/dev/null
    fi
    ;;
run)
    ;;
*)
    echo "用法：$0 [build|run|clean]"
    exit 1
    ;;
esac

# 启动
cd "$SCRIPT_DIR/build" || exit 1
if [ ! -x ./deskmon ]; then
    echo "错误：找不到可执行文件 $SCRIPT_DIR/build/deskmon，请先执行 ./start.sh build"
    exit 1
fi

echo "启动 DeskMon..."
exec ./deskmon
