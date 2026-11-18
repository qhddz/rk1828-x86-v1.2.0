# RK1828 Linux x86_64 一键安装与推理 Demo

[English](README.md) | 中文

在 **普通 x86_64 PC（Ubuntu 22.04/24.04）** 上安装瑞芯微 **RK1828** AI 协处理器（PCIe 接口，M.2 / SODIMM 模组）的全部软件栈：

- PCIe 驱动（`pcie-rkep`，DKMS 自动编译，内核升级自动重编）
- RK1828 固件（含 SODIMM / M.2 / M.2-2242 三种版型，脚本自动回退尝试）
- RKNN3 Runtime（C/C++ 推理库）、`rknn-smi`、`rknn-console`、`rkllm3-server`
- Python SDK（`rknn3-toolkit` 1.2.0，PC 端模型转换，ONNX/PyTorch → RKNN）
- 一个可直接运行的 **MobileNetV2 图像分类 demo**（C API 推理 + Top-5 输出）

软件版本：**RKNN3 SDK V1.2.0**（首个支持 Linux x86_64 主机协处理器模式的版本）。
注意 RKNN3 与老的 RKNN-Toolkit2（RK3588 用）**完全不兼容**。

## 环境要求

- x86_64 PC，Ubuntu 22.04 / 24.04（其它 Debian 系大概率可行）
- 内核 ≥ 5.10，已安装对应内核头文件（脚本会自动装）
- RK1828 模组插入 PCIe 插槽 / M.2 插槽，`lspci` 能看到：
  `Processing accelerators: Rockchip Electronics Co., Ltd Device 182a`

## 快速开始

```bash
git clone <本仓库地址> rk1828_setup   # 仓库约 155MB（含全部官方二进制，无需另行下载）
cd rk1828_setup

# 一键安装（驱动/固件/Runtime/SMI/Python SDK）
sudo ./install_all.sh
```

> 仓库已包含全部所需二进制（官方 deb、固件、Python wheel、模型文件）。
> 若文件损坏或官方发布新版，可运行 `./download_packages.sh` 重新下载（含 MD5 校验）。

`install_all.sh` 会自动完成：安装依赖 → `dpkg` 安装官方 x86_64 DEB（DKMS 编译驱动）→
检查 `/dev/pcie-rkep-*` → 烧录固件并等待设备上线（默认固件失败时自动换 M.2 版型固件）→
`rknn-smi` 验证 → 创建 `~/rknn3-venv` 并安装 Python SDK。

### 3. 关键一步：关闭 IOMMU 对 DMA 的地址转换

**x86 主板的 Intel VT-d 会导致固件烧录失败**（现象：`Device request state: 0xffa70000`
或 `check ddr load addr request failed`，`rknn-smi` 芯片信息全 NA）。
原因是固件通过 EP→主机 DMA 传输，驱动按物理地址寻址，被 IOMMU 拦截/错译。

检查：`ls /sys/class/iommu/` 若有 `dmar0/dmar1` 说明 VT-d 已启用，需要：

```bash
sudo sed -i 's/^GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"/GRUB_CMDLINE_LINUX_DEFAULT="quiet splash iommu=pt"/' /etc/default/grub
sudo update-grub
sudo reboot
```

> 新内核注意：若系统里有多个内核版本，重启会进最新内核。DKMS 会为已装驱动的内核自动重编，
> 前提是装了对应版本的 `linux-headers-*`。

### 4. 验证

```bash
sudo rknn-smi info
```

正常输出应包含芯片名、温度和显存（而不是 NA）：

```
| 0             Online   | OK            | NA            | 0                    |
| 0             RK1828   | 0000:02:00.0  | 39            | 32   / 5120          |
```

### 5. 运行推理 demo

```bash
cd demo
./run_demo.sh                # 用自带测试图
./run_demo.sh 你的图片.jpg    # 用自己的图片
```

实测输出（Intel Alder Lake-N PC）：

```
Device: type=PCIE id=0000:02:00.0
Model loaded, cost 824.52 ms
Inference time: 4.46 ms
Top-5:
  [ 830]  18.98%  stretcher
  [ 907]  17.41%  wine bottle
  ...
```

## 目录结构

```
rk1828_setup/
├── install_all.sh          # 一键安装：驱动+固件+Runtime+SMI+Python SDK
├── download_packages.sh    # 可选：重新下载官方二进制（含 MD5 校验）
├── rknn3-rk182x-sodimm_v1.2.0_x86_64.deb   # 官方 x86_64 主安装包(驱动/Runtime/SMI)
├── rknn3-rk182x-m2_v1.2.0_arm64.deb        # M.2 固件来源包
├── rknn3-rk182x-m2-2242_v1.2.0_arm64.deb   # M.2-2242 固件来源包
├── rknn3_rk1820_m2.img                     # M.2 版型固件
├── rknn3_rk1820_m2-2242.img                # M.2-2242 版型固件
├── rknn3_toolkit-1.2.0-..._x86_64.whl      # Python SDK (rknn3-toolkit 1.2.0)
├── requirements_cp312.txt  # rknn3-toolkit Python 依赖
├── quickstart_cn.pdf       # 官方 RKNN3 SDK 快速入门
└── demo/                   # MobileNetV2 图像分类 demo
    ├── include/            #   C SDK 头文件 (rknn3_api.h 等)
    ├── mobilenetv2-7.onnx  #   源模型
    ├── mobilenetv2_rk1828.rknn / .weight   # 已转好的 RKNN3 模型(可直接推理)
    ├── imagenet_labels.txt #   ImageNet 1000 类标签
    ├── convert_model.py    #   ONNX → RKNN3 模型转换（Python SDK，在 venv 中运行）
    ├── prep_image.py       #   图片预处理为 224x224 RGB 裸数据
    ├── cls_demo.c          #   C API 推理 demo（softmax + Top-5）
    ├── build.sh            #   编译 demo（gcc + /usr/lib/librknn3_api.so）
    ├── run_demo.sh         #   一键运行
    └── post_reboot_check.sh#   开机自动验证+demo（配合 systemd 服务，可选）
```

## 各组件说明

### 驱动（pcie-rkep）

DEB 包内置 DKMS 源码（`/usr/src/pcie-rkep-3.3.1/`），安装时针对当前内核自动编译，
生成 `/dev/pcie-rkep-<PCI地址>`。查看状态：`dkms status`、`modinfo pcie-rkep`。

### 固件版型

固件必须与模组版型一致（SODIMM / M.2 / M.2-2242 互不通用），文件位于
`/usr/lib/firmware/rknn3_rk1820.img`。`install_all.sh` 先尝试包内默认（SODIMM）固件，
失败自动切换 `rknn3_rk1820_m2.img`；如需 2242，手动：

```bash
sudo cp rknn3_rk1820_m2-2242.img /usr/lib/firmware/rknn3_rk1820.img
sudo systemctl restart rknn3.service
```

### Python SDK

安装在 `~/rknn3-venv`：`source ~/rknn3-venv/bin/activate` 后使用。
用于 **PC 端模型转换**（参考 `demo/convert_model.py`）。
板端推理的 `rknn3-toolkit-lite` 官方目前只发布 aarch64 版，x86_64 主机上请用
C API 或 `rkllm3-server`（OpenAI 兼容接口）。

### C SDK

头文件：已内嵌在 `demo/include/`（与官方 `airockchip/rknn3-toolkit` 仓库
`rknn3-runtime/rknn3-api/include/` 一致）；
库：安装 DEB 后在 `/usr/lib/librknn3_api.so`。调用顺序参考 `demo/cls_demo.c`：

```
rknn3_find_devices → rknn3_init → rknn3_load_model_from_path
→ rknn3_model_init(注意 core_mask 必须与转换时的 core_num 匹配)
→ rknn3_create_mem / rknn3_mem_sync → rknn3_run
```

### 开机自检（可选）

`demo/post_reboot_check.sh` + 一个 systemd oneshot 服务可实现"重启后自动等固件、
验证并跑 demo"，日志写入 `demo/last_run.log`：

```bash
sudo tee /etc/systemd/system/rk1828-demo-check.service <<EOF
[Unit]
Description=RK1828 post-reboot firmware check and demo run
After=rknn3.service
[Service]
Type=oneshot
ExecStart=$PWD/demo/post_reboot_check.sh
TimeoutStartSec=600
[Install]
WantedBy=multi-user.target
EOF
sudo systemctl enable rk1828-demo-check.service
```

## 常见问题

| 现象 | 原因与解决 |
|---|---|
| `rknn-smi` 芯片信息全 NA、日志 `0xffa70000` / `check ddr load addr request failed` | Intel VT-d 拦截 EP DMA → 按上文加 `iommu=pt` 并重启 |
| 固件烧录失败但 PCIe 设备可见 | 固件版型不对 → 换 M.2 / M.2-2242 固件重启 `rknn3.service` |
| 推理程序查询返回 `n_input=0` | 漏了 `rknn3_model_init()`（在 load_model 之后必须调用） |
| `rknn3_model_init` 报 core_mask 错误 | mask 必须与转换模型的 `core_num` 一致（core_num=1 → `RKNN3_NPU_CORE_0`） |
| 升级内核后驱动消失 | DKMS 会自动重编；若无对应 `linux-headers-*`，手动安装后 `sudo dkms autoinstall` |
| GitHub 下载慢 | `download_packages.sh` 内置 gh-proxy 回退；也可设 `https_proxy` |

## 卸载

```bash
sudo dpkg -P rknn3-rk182x-sodimm   # 会同步注销 DKMS 驱动
sudo systemctl disable rk1828-demo-check.service 2>/dev/null
```

## 资源链接

- RKNN3 SDK 官方仓库（文档/C SDK/Python wheel）: https://github.com/airockchip/rknn3-toolkit
- 模型转换示例: https://github.com/airockchip/rknn3-model-zoo
- 官方安装包网盘: https://meta.box.lenovo.com/v/link/view/a6c6ceb604454252b7998be0f343c9f9
  （链接若失效，请向瑞芯微销售/FAE 索取最新 RK1820/RK1828 RELEASE 包）
