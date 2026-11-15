#!/bin/bash
# 下载 RK1828 安装所需的大文件（GitHub 仓库不含这些文件，clone 后先运行本脚本）
# 来源: 瑞芯微官方联想网盘 + airockchip/rknn3-toolkit GitHub 仓库
set -e
cd "$(dirname "$0")"

BOX="https://console.box.lenovo.com/v2/delivery/dl_router/a6c6ceb604454252b7998be0f343c9f9/?token=&LBC=meta&aid=957121"
NSID="5314673"

dl() { # dl <neid> <output> [md5]
    echo ">> 下载 $2"
    curl -sL --retry 3 -o "$2" "${BOX}&neid=$1&nsid=${NSID}"
    if [ -n "${3:-}" ]; then
        echo "$3  $2" | md5sum -c - || { echo "MD5 校验失败: $2"; exit 1; }
    fi
}

# 1. x86_64 主安装包 (驱动/固件/Runtime/SMI)
dl 11915504816 rknn3-rk182x-sodimm_v1.2.0_x86_64.deb 73768ef5b89506d89e9665359247cdbc

# 2. M.2 版型固件 (从 arm64 包中提取，固件与架构无关)
dl 11915503238 rknn3-rk182x-m2_v1.2.0_arm64.deb
mkdir -p m2_deb && dpkg-deb -x rknn3-rk182x-m2_v1.2.0_arm64.deb m2_deb
cp m2_deb/usr/lib/firmware/rknn3_rk1820.img rknn3_rk1820_m2.img
echo "45dd70f436ae86af5283d01cc7bcb601  rknn3_rk1820_m2.img" | md5sum -c -

# 3. M.2-2242 版型固件 (备用)
dl 11915503232 rknn3-rk182x-m2-2242_v1.2.0_arm64.deb
mkdir -p m22242_deb && dpkg-deb -x rknn3-rk182x-m2-2242_v1.2.0_arm64.deb m22242_deb

# 4. RKNN3-Toolkit Python wheel (cp312, x86_64, ~101MB)
WHEEL=rknn3_toolkit-1.2.0-cp312-cp312-manylinux2014_x86_64.whl
if [ ! -f "$WHEEL" ]; then
    echo ">> 下载 $WHEEL"
    curl -sL --retry 3 -o "$WHEEL" \
        "https://raw.githubusercontent.com/airockchip/rknn3-toolkit/main/rknn3-toolkit/packages/$WHEEL" \
      || curl -sL --retry 3 -o "$WHEEL" \
        "https://gh-proxy.com/https://raw.githubusercontent.com/airockchip/rknn3-toolkit/main/rknn3-toolkit/packages/$WHEEL"
    echo "532bcb4416dc4083ce8eb717ff95928f  $WHEEL" | md5sum -c -
fi

# 5. toolkit 依赖清单
[ -f requirements_cp312.txt ] || curl -sL -o requirements_cp312.txt \
    "https://gh-proxy.com/https://raw.githubusercontent.com/airockchip/rknn3-toolkit/main/rknn3-toolkit/packages/requirements_cp312-1.2.0.txt"

echo "== 全部下载完成，接下来执行: sudo ./install_all.sh =="
