#!/bin/bash
# RK1828 (PCIe) 一键安装脚本：依赖 + x86_64 DEB(驱动/固件/Runtime/SMI) + 固件版型自适应
# 用法: sudo /path/to/repo/install_all.sh
set -u
SETUP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SETUP_DIR"

echo "===== [1/5] 安装系统依赖 ====="
apt-get update || true
apt-get install -y dkms adb libusb-1.0-0 build-essential python3.12-venv python3-pip "linux-headers-$(uname -r)" || true

echo "===== [2/5] 安装 RKNN3 x86_64 DEB 包(含 DKMS 驱动) ====="
dpkg -i rknn3-rk182x-sodimm_v1.2.0_x86_64.deb || apt-get -f install -y

echo "===== [3/5] 检查 PCIe 驱动 ====="
dkms status | grep -i pcie-rkep || echo "WARN: dkms 中未见 pcie-rkep"
modinfo pcie-rkep 2>/dev/null | head -4 || true
modprobe pcie-rkep 2>/dev/null || true
sleep 2
ls -l /dev/pcie-rkep-* || { echo "ERROR: 无 /dev/pcie-rkep-* 设备节点，驱动安装失败"; exit 1; }

echo "===== [4/5] 启动服务并等待设备上线(默认固件) ====="
wait_online() {
    # 注意: 固件未启动时 rknn-smi 也会显示 Online，必须看芯片名
    for i in $(seq 1 40); do
        if rknn-smi info 2>/dev/null | grep -qE "RK182[08]"; then
            return 0
        fi
        sleep 3
    done
    return 1
}

systemctl restart rknn3.service rknn-mdns.service 2>/dev/null || true
if wait_online; then
    echo ">>> 默认(SODIMM)固件启动成功"
else
    [ -f /usr/lib/firmware/rknn3_rk1820.img.sodimm.bak ] || \
        cp /usr/lib/firmware/rknn3_rk1820.img /usr/lib/firmware/rknn3_rk1820.img.sodimm.bak
    OK=0
    for fw in rknn3_rk1820_m2.img rknn3_rk1820_m2-2242.img; do
        [ -f "$fw" ] || continue
        echo ">>> 默认固件未能上线，切换为 $fw 重试..."
        cp "$fw" /usr/lib/firmware/rknn3_rk1820.img
        systemctl restart rknn3.service
        if wait_online; then
            echo ">>> $fw 启动成功"
            OK=1
            break
        fi
    done
    if [ "$OK" != "1" ]; then
        echo "ERROR: 所有版型固件均未能使设备上线，请检查硬件连接"
        exit 1
    fi
fi

echo "===== [5/5] 验证 ====="
rknn-smi info
echo "----------------------------------------"
rknn-smi -v

echo "===== [附加] 安装 Python SDK 到 ~/rknn3-venv ====="
REAL_USER=${SUDO_USER:-$(logname 2>/dev/null || echo dfrobot)}
su - "$REAL_USER" -c "
set -e
cd '$SETUP_DIR'
python3 -m venv ~/rknn3-venv
~/rknn3-venv/bin/pip install -q --index-url https://pypi.tuna.tsinghua.edu.cn/simple -r requirements_cp312.txt
~/rknn3-venv/bin/pip install -q rknn3_toolkit-1.2.0-cp312-cp312-manylinux2014_x86_64.whl
~/rknn3-venv/bin/python -c \"from rknn.api import RKNN; print('rknn3-toolkit Python SDK OK')\"
"

echo "===== 全部安装完成 ====="
