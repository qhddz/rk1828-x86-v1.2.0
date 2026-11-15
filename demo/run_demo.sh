#!/bin/bash
# RK1828 分类 demo 一键运行（设备固件正常启动后使用）
set -e
cd "$(dirname "$0")"

IMG=${1:-/usr/share/rknn3-rk182x/test.jpg}

echo "== 检查设备状态 =="
sudo rknn-smi info | grep -E "RK182[08]" || { echo "设备未就绪，请先用 rknn-smi info 确认"; exit 1; }

[ -f mobilenetv2_rk1828.rknn ] || ~/rknn3-venv/bin/python convert_model.py mobilenetv2-7.onnx mobilenetv2_rk1828.rknn
[ -f input.bin ] || ~/rknn3-venv/bin/python prep_image.py "$IMG" input.bin
[ -x cls_demo ] || ./build.sh

echo "== 运行推理 =="
./cls_demo mobilenetv2_rk1828.rknn mobilenetv2_rk1828.weight input.bin imagenet_labels.txt
