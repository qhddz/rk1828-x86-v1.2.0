#!/usr/bin/env python3
"""把任意图片预处理成 224x224 uint8 RGB 裸数据文件（NHWC），供 cls_demo 使用。

用法: python3 prep_image.py <image_path> <output_bin>
"""
import sys

from PIL import Image

SIZE = 224


def main():
    if len(sys.argv) < 3:
        print(f"Usage: python3 {sys.argv[0]} image_path output_bin")
        sys.exit(1)

    img = Image.open(sys.argv[1]).convert('RGB')

    # 等比缩放后中心裁剪到 224x224（与 torchvision 预处理一致）
    w, h = img.size
    scale = 256.0 / min(w, h)
    img = img.resize((round(w * scale), round(h * scale)), Image.BILINEAR)
    left = (img.width - SIZE) // 2
    top = (img.height - SIZE) // 2
    img = img.crop((left, top, left + SIZE, top + SIZE))

    data = img.tobytes()
    with open(sys.argv[2], 'wb') as f:
        f.write(data)
    print(f'saved {sys.argv[2]}: {len(data)} bytes ({SIZE}x{SIZE}x3 RGB)')


if __name__ == '__main__':
    main()
