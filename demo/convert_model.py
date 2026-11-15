#!/usr/bin/env python3
"""将 ONNX 分类模型转换为 RK1828 可用的 RKNN3 模型（不量化，免校准数据集）。

用法: python3 convert_model.py <onnx_path> [output_rknn_path]
产物: <output>.rknn + <output>.weight
"""
import os
import sys

from rknn.api import RKNN


def main():
    if len(sys.argv) < 2:
        print(f"Usage: python3 {sys.argv[0]} onnx_model_path [output_rknn_path]")
        sys.exit(1)

    model_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(model_path)[0] + '.rknn'

    rknn = RKNN(verbose=False)

    # ImageNet 归一化内嵌进模型: 输入直接喂 uint8 RGB (NHWC)
    # mean/std = torchvision 标准值 * 255
    print('--> Config model')
    rknn.config(
        mean_values=[[123.675, 116.28, 103.53]],
        std_values=[[58.395, 57.12, 57.375]],
        target_platform='rk1828',
        input_attrs={'data': {'dtype': 'uint8', 'layout': 'NHWC'}},
        core_num=1,
    )
    print('done')

    print('--> Loading model')
    ret = rknn.load_onnx(model=model_path)
    if ret != 0:
        print('Load model failed!')
        sys.exit(ret)
    print('done')

    print('--> Building model (fp, no quantization)')
    ret = rknn.build(do_quantization=False)
    if ret != 0:
        print('Build model failed!')
        sys.exit(ret)
    print('done')

    print('--> Export rknn model')
    ret = rknn.export_rknn(output_path)
    if ret != 0:
        print('Export rknn model failed!')
        sys.exit(ret)
    print('done:', output_path)

    rknn.release()


if __name__ == '__main__':
    main()
