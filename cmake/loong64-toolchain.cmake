# DeskMon LoongArch64 (loong64) 交叉编译工具链文件
# 用法：cmake -DCMAKE_TOOLCHAIN_FILE=cmake/loong64-toolchain.cmake ...
#
# 依赖系统源里的交叉编译器（apt: gcc/g++-loongarch64-linux-gnu），
# loong64 版 Qt6/DTK6 dev 包装在 /usr/lib/loong64-linux-gnu，
# CMake 多架构查找开 ONLY 即可命中，无需手填 sysroot。
set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR loong64)

set(CMAKE_C_COMPILER   loongarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER loongarch64-linux-gnu-g++)

# 只在目标根路径下查找库/头/包，避免误命中本机 amd64 版本
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
