/*
 * RK1828 图像分类 demo (RKNN3 C API)
 *
 * 用法: cls_demo <model.rknn> <model.weight> <input.bin> [labels.txt]
 *   input.bin 为 224x224x3 uint8 RGB 裸数据(由 prep_image.py 生成)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rknn3_api.h"

#define MAX_CLASSES 2048
#define TOP_K 5

static double elapsed_ms(struct timespec* a, struct timespec* b)
{
    return (b->tv_sec - a->tv_sec) * 1000.0 + (b->tv_nsec - a->tv_nsec) / 1e6;
}

static float half_to_float(unsigned short h)
{
    unsigned sign = (h >> 15) & 1;
    int exp = (h >> 10) & 0x1f;
    unsigned mant = h & 0x3ff;
    float val;
    if (exp == 0) {
        val = ldexpf((float)mant, -24);
    } else if (exp == 31) {
        val = mant ? NAN : INFINITY;
    } else {
        val = ldexpf((float)(mant | 0x400), exp - 25);
    }
    return sign ? -val : val;
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        printf("Usage: %s <model.rknn> <model.weight> <input.bin> [labels.txt]\n", argv[0]);
        return -1;
    }
    const char* model_path  = argv[1];
    const char* weight_path = argv[2];
    const char* input_path  = argv[3];
    const char* labels_path = argc > 4 ? argv[4] : NULL;

    /* 读取标签 */
    char labels[MAX_CLASSES][128];
    int n_labels = 0;
    if (labels_path) {
        FILE* fp = fopen(labels_path, "r");
        if (fp) {
            while (n_labels < MAX_CLASSES && fgets(labels[n_labels], sizeof(labels[0]), fp)) {
                labels[n_labels][strcspn(labels[n_labels], "\r\n")] = 0;
                n_labels++;
            }
            fclose(fp);
        }
    }

    /* 发现设备并初始化 */
    rknn3_devices devs;
    memset(&devs, 0, sizeof(devs));
    int ret = rknn3_find_devices(&devs);
    if (ret != RKNN3_SUCCESS || devs.n_devices == 0) {
        printf("No RK182X device found! ret=%d n=%d\n", ret, devs.n_devices);
        return -1;
    }
    printf("Device: type=%s id=%s\n", devs.devices[0].type, devs.devices[0].id);

    rknn3_context ctx;
    rknn3_init_extend init_extend = {0};
    init_extend.device_id = devs.devices[0].id;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ret = rknn3_init(&ctx, &init_extend);
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_init failed! ret=%d\n", ret);
        return -1;
    }

    /* 加载模型 */
    ret = rknn3_load_model_from_path(ctx, model_path, weight_path);
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_load_model_from_path failed! ret=%d\n", ret);
        rknn3_destroy(ctx);
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Model loaded, cost %.2f ms\n", elapsed_ms(&t0, &t1));

    /* 初始化模型(分配 NPU 资源) */
    rknn3_config config;
    memset(&config, 0, sizeof(config));
    config.run_core_mask = RKNN3_NPU_CORE_0;
    ret = rknn3_model_init(ctx, &config);
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_model_init failed! ret=%d\n", ret);
        rknn3_destroy(ctx);
        return -1;
    }

    /* 查询 IO */
    rknn3_input_output_num io_num;
    memset(&io_num, 0, sizeof(io_num));
    ret = rknn3_query(ctx, RKNN3_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN3_SUCCESS || io_num.n_input != 1) {
        printf("Query IO num failed or unexpected input count! ret=%d n_in=%d\n", ret, io_num.n_input);
        rknn3_destroy(ctx);
        return -1;
    }

    rknn3_shape_info shape;
    memset(&shape, 0, sizeof(shape));
    shape.shape_id     = 0;
    shape.input_attrs  = (rknn3_tensor_attr*)calloc(io_num.n_input, sizeof(rknn3_tensor_attr));
    shape.output_attrs = (rknn3_tensor_attr*)calloc(io_num.n_output, sizeof(rknn3_tensor_attr));
    ret = rknn3_query(ctx, RKNN3_QUERY_DYNAMIC_SHAPE_INFO, &shape, sizeof(shape));
    if (ret != RKNN3_SUCCESS) {
        printf("Query shape info failed! ret=%d\n", ret);
        rknn3_destroy(ctx);
        return -1;
    }

    rknn3_tensor_attr* in_attr  = &shape.input_attrs[0];
    rknn3_tensor_attr* out_attr = &shape.output_attrs[0];
    printf("Input : name=%s dtype=%d layout=%d shape=[", in_attr->name, in_attr->dtype, in_attr->layout);
    for (uint32_t i = 0; i < in_attr->n_dims; i++) printf("%d%s", in_attr->shape[i], i + 1 < in_attr->n_dims ? "," : "");
    printf("] aligned=%lu bytes\n", in_attr->aligned_size);
    printf("Output: name=%s dtype=%d n_elems=%u aligned=%lu bytes\n", out_attr->name, out_attr->dtype, out_attr->n_elems, out_attr->aligned_size);

    /* 读取输入文件 */
    FILE* fp = fopen(input_path, "rb");
    if (!fp) {
        printf("Cannot open input file %s\n", input_path);
        rknn3_destroy(ctx);
        return -1;
    }
    rknn3_tensor input  = {0};
    rknn3_tensor output = {0};
    input.attr  = in_attr;
    output.attr = out_attr;
    input.mem  = rknn3_create_mem(ctx, in_attr->aligned_size, in_attr->core_id, RKNN3_FLAG_MEMORY_CACHEABLE);
    output.mem = rknn3_create_mem(ctx, out_attr->aligned_size, out_attr->core_id, RKNN3_FLAG_MEMORY_CACHEABLE);
    if (!input.mem || !output.mem) {
        printf("rknn3_create_mem failed!\n");
        rknn3_destroy(ctx);
        return -1;
    }
    size_t nread = fread(input.mem->virt_addr, 1, input.mem->size, fp);
    fclose(fp);
    printf("Input data: %zu bytes\n", nread);

    /* 推理 */
    rknn3_mem_sync(ctx, input.mem, RKNN3_MEMORY_SYNC_TO_DEVICE);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ret = rknn3_run(ctx, &input, 1, &output, 1);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_run failed! ret=%d\n", ret);
        rknn3_destroy_mem(ctx, input.mem);
        rknn3_destroy_mem(ctx, output.mem);
        rknn3_destroy(ctx);
        return -1;
    }
    rknn3_mem_sync(ctx, output.mem, RKNN3_MEMORY_SYNC_FROM_DEVICE);
    printf("Inference time: %.2f ms\n", elapsed_ms(&t0, &t1));

    /* 输出转 fp32 */
    uint32_t n = out_attr->n_elems;
    float* scores = (float*)malloc(n * sizeof(float));
    if (out_attr->dtype == RKNN3_TENSOR_FLOAT32) {
        memcpy(scores, output.mem->virt_addr, n * sizeof(float));
    } else if (out_attr->dtype == RKNN3_TENSOR_FLOAT16) {
        unsigned short* p = (unsigned short*)output.mem->virt_addr;
        for (uint32_t i = 0; i < n; i++) scores[i] = half_to_float(p[i]);
    } else if (out_attr->dtype == RKNN3_TENSOR_INT8) {
        int8_t* p = (int8_t*)output.mem->virt_addr;
        float scale = out_attr->qnt_info.scale;
        int32_t zp  = out_attr->qnt_info.zero_point;
        for (uint32_t i = 0; i < n; i++) scores[i] = (p[i] - zp) * scale;
    } else {
        printf("Unsupported output dtype %d\n", out_attr->dtype);
        rknn3_destroy(ctx);
        return -1;
    }

    /* softmax + top5 */
    float max_v = scores[0];
    for (uint32_t i = 1; i < n; i++) if (scores[i] > max_v) max_v = scores[i];
    double sum = 0;
    for (uint32_t i = 0; i < n; i++) sum += exp((double)(scores[i] - max_v));

    printf("\nTop-%d:\n", TOP_K);
    for (int k = 0; k < TOP_K; k++) {
        uint32_t best = 0;
        for (uint32_t i = 1; i < n; i++) if (scores[i] > scores[best]) best = i;
        float prob = (float)(exp((double)(scores[best] - max_v)) / sum);
        const char* name = (int)best < n_labels ? labels[best] : "";
        printf("  [%4u] %6.2f%%  %s\n", best, prob * 100.0f, name);
        scores[best] = -1e30f;
    }

    free(scores);
    free(shape.input_attrs);
    free(shape.output_attrs);
    rknn3_destroy_mem(ctx, input.mem);
    rknn3_destroy_mem(ctx, output.mem);
    rknn3_destroy(ctx);
    return 0;
}
