#!/bin/bash
# 重启后自动检查: 等待固件上线 -> rknn-smi 验证 -> 跑分类 demo
# 由 rk1828-demo-check.service 调用(root 运行), 日志写入 demo/last_run.log

DEMO_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$DEMO_DIR/last_run.log"
exec > >(tee -a "$LOG") 2>&1

echo ""
echo "================ $(date) ================"

echo "== [1/3] 等待 RK1828 固件上线(最长 5 分钟) =="
READY=0
for i in $(seq 1 60); do
    if rknn-smi info 2>/dev/null | grep -q "RK1828"; then
        READY=1
        break
    fi
    sleep 5
done

echo "== [2/3] 设备状态 =="
rknn-smi info || true
rknn-smi -v || true

if [ "$READY" != "1" ]; then
    echo "!! 固件仍未上线, 跳过 demo. 请把本日志发给分析人员."
    exit 1
fi

echo "== [3/3] 运行分类 demo =="
cd "$DEMO_DIR"
./cls_demo mobilenetv2_rk1828.rknn mobilenetv2_rk1828.weight input.bin imagenet_labels.txt || true

echo "================ 检查结束 ================"
chown --reference="$DEMO_DIR" "$LOG" 2>/dev/null || true
