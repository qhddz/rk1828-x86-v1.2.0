#!/bin/bash
# 编译 cls_demo (x86_64)
set -e
cd "$(dirname "$0")"
# 优先使用仓库内嵌头文件；没有则回退到官方仓库克隆路径
INC=./include
[ -f "$INC/rknn3_api.h" ] || INC=~/rk1828_setup/rknn3-toolkit/rknn3-runtime/rknn3-api/include
gcc -O2 -o cls_demo cls_demo.c \
    -I"$INC" \
    -L/usr/lib -lrknn3_api -lm -Wl,-rpath,/usr/lib
echo "build ok: $(pwd)/cls_demo"
