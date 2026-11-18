# RK1828 Linux x86_64 One-Click Install & Inference Demo

English | [中文](README_CN.md)

Install the complete software stack for the Rockchip **RK1828** AI coprocessor
(PCIe, M.2 / SODIMM module) on a **plain x86_64 PC (Ubuntu 22.04/24.04)**:

- PCIe driver (`pcie-rkep`, built via DKMS — auto-rebuilt on kernel upgrades)
- RK1828 firmware (SODIMM / M.2 / M.2-2242 variants, with automatic fallback)
- RKNN3 Runtime (C/C++ inference library), `rknn-smi`, `rknn-console`, `rkllm3-server`
- Python SDK (`rknn3-toolkit` 1.2.0, PC-side model conversion, ONNX/PyTorch → RKNN)
- A ready-to-run **MobileNetV2 classification demo** (C API inference + Top-5)

Software version: **RKNN3 SDK V1.2.0** (the first release supporting Linux
x86_64 hosts in coprocessor mode). Note that RKNN3 is **completely
incompatible** with the legacy RKNN-Toolkit2 (for RK3588).

## Requirements

- x86_64 PC, Ubuntu 22.04 / 24.04 (other Debian-based distros will likely work)
- Kernel ≥ 5.10 with matching kernel headers (the script installs them)
- RK1828 module in a PCIe / M.2 slot; `lspci` should show:
  `Processing accelerators: Rockchip Electronics Co., Ltd Device 182a`

## Quick start

```bash
git clone <this repo> rk1828_setup   # ~155MB, all official binaries included
cd rk1828_setup

# one-click install (driver / firmware / runtime / SMI / Python SDK)
sudo ./install_all.sh
```

> The repo ships every required binary (official debs, firmware, Python wheel,
> model files). If a file is corrupted or Rockchip publishes a newer release,
> re-download with `./download_packages.sh` (MD5-verified).

`install_all.sh` automatically: installs dependencies → `dpkg`-installs the
official x86_64 DEB (DKMS builds the driver) → checks `/dev/pcie-rkep-*` →
flashes the firmware and waits for the device (falls back to the M.2 firmware
if the default fails) → verifies with `rknn-smi` → creates `~/rknn3-venv` and
installs the Python SDK.

### 3. Critical step: disable IOMMU DMA address translation

**Intel VT-d on x86 motherboards breaks firmware flashing** (symptoms:
`Device request state: 0xffa70000` or `check ddr load addr request failed`,
`rknn-smi` shows all-NA chip info). The firmware is transferred via EP→host
DMA using physical addresses, which the IOMMU intercepts/mistranslates.

Check: if `ls /sys/class/iommu/` shows `dmar0/dmar1`, VT-d is enabled and you
need:

```bash
sudo sed -i 's/^GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"/GRUB_CMDLINE_LINUX_DEFAULT="quiet splash iommu=pt"/' /etc/default/grub
sudo update-grub
sudo reboot
```

> Multi-kernel note: with several kernels installed, the reboot picks the
> newest one. DKMS rebuilds the driver automatically for every kernel that
> has matching `linux-headers-*` installed.

### 4. Verify

```bash
sudo rknn-smi info
```

Healthy output shows chip name, temperature and memory (not NA):

```
| 0             Online   | OK            | NA            | 0                    |
| 0             RK1828   | 0000:02:00.0  | 39            | 32   / 5120          |
```

### 5. Run the inference demo

```bash
cd demo
./run_demo.sh                # bundled test image
./run_demo.sh your_image.jpg # your own image
```

Measured output (Intel Alder Lake-N PC):

```
Device: type=PCIE id=0000:02:00.0
Model loaded, cost 824.52 ms
Inference time: 4.46 ms
Top-5:
  [ 830]  18.98%  stretcher
  [ 907]  17.41%  wine bottle
  ...
```

## Repository layout

```
rk1828_setup/
├── install_all.sh          # one-click install: driver+firmware+runtime+SMI+Python SDK
├── download_packages.sh    # optional: re-download official binaries (MD5-verified)
├── rknn3-rk182x-sodimm_v1.2.0_x86_64.deb   # official x86_64 package (driver/runtime/SMI)
├── rknn3-rk182x-m2_v1.2.0_arm64.deb        # M.2 firmware source package
├── rknn3-rk182x-m2-2242_v1.2.0_arm64.deb   # M.2-2242 firmware source package
├── rknn3_rk1820_m2.img                     # M.2 firmware
├── rknn3_rk1820_m2-2242.img                # M.2-2242 firmware
├── rknn3_toolkit-1.2.0-..._x86_64.whl      # Python SDK (rknn3-toolkit 1.2.0)
├── requirements_cp312.txt  # rknn3-toolkit Python dependencies
├── quickstart_cn.pdf       # official RKNN3 SDK quickstart (Chinese)
└── demo/                   # MobileNetV2 classification demo
    ├── include/            #   C SDK headers (rknn3_api.h etc.)
    ├── mobilenetv2-7.onnx  #   source model
    ├── mobilenetv2_rk1828.rknn / .weight   # pre-converted RKNN3 model (ready to run)
    ├── imagenet_labels.txt #   ImageNet 1000-class labels
    ├── convert_model.py    #   ONNX → RKNN3 conversion (Python SDK, run inside the venv)
    ├── prep_image.py       #   image preprocessing to 224x224 RGB raw data
    ├── cls_demo.c          #   C API inference demo (softmax + Top-5)
    ├── build.sh            #   build the demo (gcc + /usr/lib/librknn3_api.so)
    ├── run_demo.sh         #   one-click run
    └── post_reboot_check.sh#   auto-verify+demo after boot (with a systemd unit, optional)
```

## Components

### Driver (pcie-rkep)

The DEB ships DKMS sources (`/usr/src/pcie-rkep-3.3.1/`) and builds against
the running kernel at install time, creating `/dev/pcie-rkep-<PCI-addr>`.
Check status with `dkms status` and `modinfo pcie-rkep`.

### Firmware variants

The firmware must match the module form factor (SODIMM / M.2 / M.2-2242 are
not interchangeable); the active file is `/usr/lib/firmware/rknn3_rk1820.img`.
`install_all.sh` tries the bundled default (SODIMM) first and falls back to
`rknn3_rk1820_m2.img` on failure. For M.2-2242, do it manually:

```bash
sudo cp rknn3_rk1820_m2-2242.img /usr/lib/firmware/rknn3_rk1820.img
sudo systemctl restart rknn3.service
```

### Python SDK

Installed into `~/rknn3-venv`; activate with `source ~/rknn3-venv/bin/activate`.
Used for **PC-side model conversion** (see `demo/convert_model.py`).
On-device inference via `rknn3-toolkit-lite` is currently published for
aarch64 only — on x86_64 hosts use the C API or `rkllm3-server`
(OpenAI-compatible endpoint).

### C SDK

Headers: bundled in `demo/include/` (identical to the official
`airockchip/rknn3-toolkit` repo, `rknn3-runtime/rknn3-api/include/`).
Library: `/usr/lib/librknn3_api.so` after the DEB install. Call sequence
(see `demo/cls_demo.c`):

```
rknn3_find_devices → rknn3_init → rknn3_load_model_from_path
→ rknn3_model_init (core_mask must match the conversion-time core_num)
→ rknn3_create_mem / rknn3_mem_sync → rknn3_run
```

### Post-reboot self-check (optional)

`demo/post_reboot_check.sh` plus a systemd oneshot unit gives you "wait for
firmware, verify, run the demo" after every boot, logging to
`demo/last_run.log`:

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

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `rknn-smi` shows all-NA chip info; log has `0xffa70000` / `check ddr load addr request failed` | Intel VT-d intercepts the EP DMA → add `iommu=pt` as above and reboot |
| Firmware flashing fails but the PCIe device is visible | Wrong firmware variant → switch to the M.2 / M.2-2242 firmware and restart `rknn3.service` |
| Inference app queries return `n_input=0` | Missing `rknn3_model_init()` (mandatory after load_model) |
| `rknn3_model_init` reports a core_mask error | The mask must match the model's conversion `core_num` (core_num=1 → `RKNN3_NPU_CORE_0`) |
| Driver gone after a kernel upgrade | DKMS rebuilds automatically; if `linux-headers-*` is missing, install it and run `sudo dkms autoinstall` |
| Slow GitHub downloads | `download_packages.sh` has a gh-proxy fallback; or set `https_proxy` |

## Uninstall

```bash
sudo dpkg -P rknn3-rk182x-sodimm   # also unregisters the DKMS driver
sudo systemctl disable rk1828-demo-check.service 2>/dev/null
```

## Resources

- Official RKNN3 SDK repo (docs / C SDK / Python wheel): https://github.com/airockchip/rknn3-toolkit
- Model conversion examples: https://github.com/airockchip/rknn3-model-zoo
- Official packages (netdisk): https://meta.box.lenovo.com/v/link/view/a6c6ceb604454252b7998be0f343c9f9
  (if the link is dead, ask your Rockchip sales/FAE for the latest RK1820/RK1828 RELEASE package)
