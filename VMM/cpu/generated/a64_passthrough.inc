enum : uint32_t { CPU_PT_STUB_COUNT = 20136 };

static const cpu_pt_leaf_t cpu_pt_leaves[] = {
    {0x00000000u, 0, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 1, CPU_HOST_FEAT_FP16, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 2, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 4, CPU_HOST_FEAT_FP16, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 5, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 6, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 7, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 8, CPU_HOST_FEAT_AES, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 9, CPU_HOST_FEAT_AES, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 10, CPU_HOST_FEAT_AES, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 11, CPU_HOST_FEAT_AES, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 12, CPU_HOST_FEAT_BF16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 13, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 14, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 15, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 16, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 17, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 18, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 19, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 20, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 21, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 22, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 23, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 24, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 25, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 26, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 27, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 28, CPU_HOST_FEAT_FP16, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 29, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 30, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 31, CPU_HOST_FEAT_FP16, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 32, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 33, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 34, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 35, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 36, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 37, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 38, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 39, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 40, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 41, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 42, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 43, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 44, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 45, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 46, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 47, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 48, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 49, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 50, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 51, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 52, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 53, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 54, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 55, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 56, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 57, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 58, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 59, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 60, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 61, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 62, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 63, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 64, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 65, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 66, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 67, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 68, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 69, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 70, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 71, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 72, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 73, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 74, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 75, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 76, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 77, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 78, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 79, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 80, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 81, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 82, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 83, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 84, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 85, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 86, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 87, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 88, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 89, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 90, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 91, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 92, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 93, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 94, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 95, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 96, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 97, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 98, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 99, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 100, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 101, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 102, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 103, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 104, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 105, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 106, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 107, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 108, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 109, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 110, CPU_HOST_FEAT_JSCVT, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 111, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 112, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 113, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 114, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 115, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 116, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 117, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 118, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 119, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 120, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 121, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 122, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 123, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 124, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 125, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 126, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 127, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 128, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 129, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 130, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 131, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 132, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 133, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 134, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 135, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 136, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 137, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 138, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 139, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 140, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 141, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 142, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 143, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 144, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 145, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 146, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 147, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 148, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 149, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 150, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 151, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 152, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 153, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 154, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 155, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 156, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 157, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 158, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 159, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 160, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 161, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 162, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 163, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 164, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 165, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 166, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 167, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 168, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 169, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 170, CPU_HOST_FEAT_LRCPC, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 171, CPU_HOST_FEAT_LRCPC, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 172, CPU_HOST_FEAT_LRCPC, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 173, CPU_HOST_FEAT_LRCPC, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 174, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 175, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 176, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 177, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 178, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 179, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 180, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 181, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 182, CPU_HOST_FEAT_SHA1, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 183, CPU_HOST_FEAT_SHA1, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 184, CPU_HOST_FEAT_SHA256, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 185, CPU_HOST_FEAT_SHA512, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 186, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 187, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 188, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 189, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 190, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 191, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 192, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 193, CPU_HOST_FEAT_BF16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 195, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 197, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 199, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 201, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 203, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 205, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 207, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 209, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 211, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 213, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 215, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 217, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 219, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 221, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 223, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 225, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 227, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 229, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 231, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 233, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 235, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 237, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 239, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 241, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 243, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 245, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 247, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 249, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 251, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 253, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 255, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 257, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 259, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 261, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 263, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 265, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 267, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 269, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 271, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 273, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 275, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 277, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 279, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 281, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 283, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 285, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 287, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 289, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 291, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 293, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 295, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 297, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 299, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 301, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 303, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 305, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 307, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 309, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 311, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 313, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 315, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00100000u, 317, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 319, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 323, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 327, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 331, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 335, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 339, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 343, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 347, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 351, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 355, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 359, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 363, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 367, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 371, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 375, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 379, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 383, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 387, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 391, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 395, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 399, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 403, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 407, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 411, CPU_HOST_FEAT_FRINTTS, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 415, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 419, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 423, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 427, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 431, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 435, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 439, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 443, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 447, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 451, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 455, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 459, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 463, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 467, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 471, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 475, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 479, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 483, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 487, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 491, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 495, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 503, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 511, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 519, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 527, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 535, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 543, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 551, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 559, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 567, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001fe000u, 575, 0, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001fe000u, 831, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001fe000u, 1087, 0, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x000703e0u, 1343, 0, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x000703e0u, 1599, 0, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x000703e0u, 1855, 0, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2111, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2119, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2127, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2135, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2143, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2151, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2159, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2167, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2175, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2183, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2191, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2199, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2207, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2215, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2223, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2231, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2239, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2247, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2255, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2263, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2271, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2279, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2287, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x400703e0u, 2295, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x400703e0u, 2807, 0, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3319, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3320, CPU_HOST_FEAT_BF16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3321, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3322, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3323, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3324, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3325, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3326, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3327, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3328, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3329, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3330, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3331, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3332, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3333, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3334, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3335, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3339, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3343, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3347, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3351, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3355, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3359, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3363, CPU_HOST_FEAT_LSE, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3367, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3368, CPU_HOST_FEAT_LSE, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3369, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3370, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3371, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3372, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3373, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3374, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3375, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3407, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3408, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3409, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3410, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3411, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3412, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3413, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3414, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3415, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3416, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3417, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3418, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3419, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3420, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3421, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3422, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3423, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3424, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3425, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3426, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3427, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3428, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3429, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3430, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3431, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3432, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3433, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3434, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3435, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3436, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3437, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3438, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3439, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3440, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3441, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3442, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3443, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3475, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3476, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3477, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3478, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3479, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3480, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3481, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3482, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3483, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3484, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3485, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3486, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3487, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3488, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3489, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3490, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3491, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3492, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3493, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3494, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3495, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3496, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3497, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3498, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3499, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3500, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3501, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3502, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3503, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3504, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3505, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3506, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3507, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3508, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3509, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3510, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3511, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3512, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3513, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3514, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3515, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3516, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3517, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3518, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3519, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3520, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3521, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3522, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3523, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3524, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3525, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3526, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3527, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3528, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3529, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3530, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3531, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3532, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3533, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3534, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3535, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3536, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3537, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3538, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3539, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3540, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3541, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3542, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3543, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3544, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3545, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3546, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3547, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3548, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3549, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3550, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3551, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3552, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3553, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3554, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3555, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3556, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3557, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3558, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3559, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3560, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3561, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3562, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3563, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3564, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3565, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3566, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3567, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3568, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3569, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3570, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3571, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3572, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3573, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3574, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3575, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3576, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3577, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3578, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3579, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3580, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3581, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3582, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3583, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3584, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3585, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3586, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3587, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3588, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3589, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3590, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3591, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3592, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3593, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3594, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3595, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3596, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3597, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3598, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3599, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3600, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3601, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3602, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3603, CPU_HOST_FEAT_SHA3, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3604, CPU_HOST_FEAT_SHA1, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3605, CPU_HOST_FEAT_SHA1, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3606, CPU_HOST_FEAT_SHA1, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3607, CPU_HOST_FEAT_SHA1, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3608, CPU_HOST_FEAT_SHA256, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3609, CPU_HOST_FEAT_SHA256, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3610, CPU_HOST_FEAT_SHA256, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3611, CPU_HOST_FEAT_SHA512, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3612, CPU_HOST_FEAT_SHA512, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3613, CPU_HOST_FEAT_SHA512, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3614, CPU_HOST_FEAT_I8MM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3615, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3647, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3679, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3680, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3681, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3682, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3683, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3684, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3685, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3686, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3687, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3688, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3689, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3690, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3691, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3692, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3693, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3694, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3695, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3696, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3697, CPU_HOST_FEAT_LSE, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3698, CPU_HOST_FEAT_I8MM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3699, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3731, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3732, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3733, CPU_HOST_FEAT_I8MM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3734, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3736, CPU_HOST_FEAT_BF16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3738, CPU_HOST_FEAT_BF16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3740, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3742, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3744, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3746, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x401f0000u, 3748, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x401f0000u, 3812, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3876, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3878, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3880, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3882, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3884, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3886, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3888, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3890, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3892, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3894, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3896, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3898, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3900, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3902, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3904, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 3906, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 3970, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4034, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4098, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4162, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4226, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4290, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4354, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4418, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4482, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4546, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4610, 0, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4674, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4676, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4678, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4680, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4682, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4684, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4686, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4688, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4690, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4692, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4694, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4696, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4698, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4700, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4702, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4704, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 4706, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4708, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4710, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 4712, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4714, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 4716, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4718, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4720, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4722, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4724, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4788, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4852, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4916, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4980, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5044, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5108, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5172, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5236, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5300, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5364, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5428, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5492, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5556, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5620, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5684, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5748, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5812, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5876, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5940, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 6004, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 6068, CPU_HOST_FEAT_I8MM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 6070, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 6134, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6198, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6326, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6454, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6582, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6710, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6838, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6966, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7094, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7222, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7350, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7478, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7606, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7734, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7862, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7866, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7870, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7874, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7878, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7882, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7886, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7890, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7894, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7898, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7902, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7906, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7910, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7914, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7918, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7922, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7926, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7930, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7934, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7942, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7946, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7954, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7958, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7966, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7970, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7978, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7982, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7986, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7990, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7994, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7998, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8002, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8006, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8010, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8014, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8018, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8022, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8026, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8030, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8034, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8038, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8042, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 8046, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 8050, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 8306, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 8562, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 8818, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9074, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9330, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9586, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9842, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10098, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10354, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10610, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10866, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11122, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11378, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11634, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11890, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12146, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12402, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12658, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12914, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13170, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13426, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13682, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13938, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14194, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14450, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14706, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14962, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15218, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15226, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15234, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 15242, CPU_HOST_FEAT_BF16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 15258, CPU_HOST_FEAT_BF16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15274, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15282, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15290, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15298, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15306, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15314, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 15322, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 15578, CPU_HOST_FEAT_FP16, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 15834, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 16090, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 16346, CPU_HOST_FEAT_FP16, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 16602, 0, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16858, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16874, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16890, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 16906, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16922, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16938, CPU_HOST_FEAT_FHM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16954, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 16970, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16986, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 17002, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 17018, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 17034, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17050, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17058, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17066, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17074, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17082, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17090, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17098, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17106, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17114, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17122, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17130, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17138, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17146, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17154, CPU_HOST_FEAT_DotProd, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17162, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17170, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17178, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17186, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17194, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17202, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17210, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17218, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17226, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17234, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17242, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17250, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17258, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17266, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17274, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17282, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17290, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17298, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17306, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17314, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17322, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17330, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17338, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17346, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17354, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17362, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17370, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 17378, CPU_HOST_FEAT_I8MM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17394, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17402, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17410, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17418, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17426, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17434, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17442, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17450, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17458, CPU_HOST_FEAT_DotProd, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17466, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17474, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17482, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17490, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17498, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17506, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17514, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17522, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17530, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17538, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17546, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17554, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17562, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17570, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17578, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 17586, CPU_HOST_FEAT_I8MM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17602, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17610, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17618, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17626, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17634, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17642, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17650, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c01000u, 17658, CPU_HOST_FEAT_FCMA, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f000u, 17674, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f000u, 17690, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f000u, 17706, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17722, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17754, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17786, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17818, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f7800u, 17850, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18362, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18394, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18426, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18458, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18490, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18522, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18554, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 18586, CPU_HOST_FEAT_SHA3, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18587, CPU_HOST_FEAT_SHA3, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x40007800u, 18588, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c01800u, 18620, CPU_HOST_FEAT_FCMA, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 18652, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18653, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18654, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18655, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18656, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18657, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18658, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18659, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18660, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18661, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18662, CPU_HOST_FEAT_FP16, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18663, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x40f00800u, 18664, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18728, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18792, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18856, CPU_HOST_FEAT_DotProd, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18920, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18984, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19048, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19112, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19176, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19240, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19304, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19368, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19432, CPU_HOST_FEAT_RDM, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19496, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19560, CPU_HOST_FEAT_DotProd, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19624, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19688, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19752, 0, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 19816, CPU_HOST_FEAT_SHA3, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f06800u, 19880, CPU_HOST_FEAT_FCMA, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
};

static inline int32_t cpu_pt_find(uint32_t word) {
    if (word & 0x20000000u) {
        if (word & 0x10000000u) {
            if (word & 0x00004000u) {
                if (word & 0x00001000u) {
                    if (word & 0x00002000u) {
                        if (word & 0x40000000u) {
                            if (word & 0x02000000u) {
                                if (word & 0x01000000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x8c808000u) == 0x0c008000u && (((word >> 19) & 15u) != 0x0u)) return 663;
                                        } else {
                                            if ((word & 0x8c808000u) == 0x0c000000u && (((word >> 19) & 15u) != 0x0u)) return 673;
                                        }
                                    } else {
                                        if ((word & 0x8c008000u) == 0x0c008000u) return 863;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8c3f0c00u) == 0x0c300800u) return 233;
                                        } else {
                                            if ((word & 0x8c3f0c00u) == 0x0c300800u) return 229;
                                        }
                                    } else {
                                        if ((word & 0x8c3f0c00u) == 0x0c200800u) return 291;
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 528;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 526;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 536;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 534;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 530;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 524;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 538;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 532;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 527;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 525;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 535;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 533;
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 529;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 523;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 537;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 531;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x02000000u) {
                            if (word & 0x00000400u) {
                                if (word & 0x00000800u) {
                                    if ((word & 0xcd208000u) == 0x4c200000u) return 715;
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0xcda00000u) == 0x4ca00000u) return 588;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd600000u) == 0x4c600000u) return 574;
                                        } else {
                                            if ((word & 0xcd400000u) == 0x4d400000u) return 645;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0xcc008000u) == 0x4c008000u) return 862;
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0xcc768800u) == 0x4c708800u) return 166;
                                            } else {
                                                if ((word & 0xcc768800u) == 0x4c708800u) return 192;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0xcc368800u) == 0x4c208800u) return 247;
                                            } else {
                                                if ((word & 0xcc368800u) == 0x4c208800u) return 254;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00080000u) {
                                            if ((word & 0xccf68800u) == 0x4cf08800u) return 25;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0xcca68800u) == 0x4c208800u) return 195;
                                            } else {
                                                if ((word & 0xcca68800u) == 0x4ca08800u) return 203;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 496;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 494;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 495;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 493;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 504;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 502;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 503;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 501;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 498;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 492;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 497;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 491;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 506;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 500;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 505;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 499;
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00002000u) {
                        if (word & 0x00800000u) {
                            if (word & 0x40000000u) {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8f208000u) == 0x0e208000u) return 592;
                                    } else {
                                        if ((word & 0x8f208000u) == 0x0e208000u) return 600;
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 512;
                                        } else {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 510;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 514;
                                        } else {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 508;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 511;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 509;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 513;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 507;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x02000000u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00000400u) {
                                        if ((word & 0xcd208000u) == 0x4c208000u) return 590;
                                    } else {
                                        if ((word & 0xcd7f8000u) == 0x4c610000u) return 89;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x01000000u) {
                                            if ((word & 0xcc000400u) == 0x4c000400u && (((word >> 19) & 15u) != 0x0u)) return 671;
                                        } else {
                                            if ((word & 0xcc200400u) == 0x4c200400u) return 598;
                                        }
                                    } else {
                                        if ((word & 0xcd000400u) == 0x4d000400u && (((word >> 19) & 15u) != 0x0u)) return 667;
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 520;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 518;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 519;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 517;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 522;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 516;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 521;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 515;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00008000u) {
                            if (word & 0x00000800u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x00080000u) {
                                        if ((word & 0xcf770400u) == 0x4e700000u) return 23;
                                    } else {
                                        if (word & 0x00100000u) {
                                            if ((word & 0xcf270400u) == 0x4e200000u) return 231;
                                        } else {
                                            if ((word & 0xcf270400u) == 0x4e200000u) return 199;
                                        }
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0xcf760400u) == 0x4e700000u) return 46;
                                        } else {
                                            if ((word & 0xcf360400u) == 0x4e200000u) return 209;
                                        }
                                    } else {
                                        if ((word & 0xcf3e0400u) == 0x4e300000u) return 227;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 173;
                                    } else {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 171;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 172;
                                    } else {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 170;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x02000000u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 716;
                                        } else {
                                            if ((word & 0x8d3f0000u) == 0x0c210000u) return 296;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8d600400u) == 0x0c600400u) return 575;
                                        } else {
                                            if ((word & 0x8d400400u) == 0x0d400400u) return 646;
                                        }
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 480;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 478;
                                            }
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 488;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 486;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 482;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 476;
                                            }
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 490;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 484;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 479;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 477;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 487;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 485;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 481;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 475;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 489;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 483;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x00002000u) {
                    if (word & 0x00001000u) {
                        if (word & 0x02000000u) {
                            if (word & 0x00000400u) {
                                if (word & 0x00000800u) {
                                    if ((word & 0xcde08000u) == 0x4ce00000u) return 371;
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0xcd200000u) == 0x4c200000u) return 710;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd600000u) == 0x4c600000u) return 370;
                                        } else {
                                            if ((word & 0xcd400000u) == 0x4d400000u) return 658;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00010000u) {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd768800u) == 0x4c708800u) return 103;
                                        } else {
                                            if ((word & 0xcd768800u) == 0x4c708800u) return 60;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd368800u) == 0x4c208800u) return 226;
                                        } else {
                                            if ((word & 0xcd368800u) == 0x4c208800u) return 213;
                                        }
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0xcdfe0800u) == 0x4ce00800u) return 174;
                                    } else {
                                        if ((word & 0xcd3e0800u) == 0x4c200800u) return 299;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 464;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 462;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 463;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 461;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 472;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 470;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 471;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 469;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 466;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 460;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 465;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 459;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 474;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 468;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 473;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 467;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x02000000u) {
                            if (word & 0x00000400u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0xcd008000u) == 0x4c000000u) return 717;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd408000u) == 0x4c400000u) return 376;
                                        } else {
                                            if ((word & 0xcd408000u) == 0x4c400000u) return 375;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0xcd608000u) == 0x4c400000u) return 382;
                                    } else {
                                        if (word & 0x01000000u) {
                                            if ((word & 0xcc408000u) == 0x4c400000u) return 657;
                                        } else {
                                            if ((word & 0xcc608000u) == 0x4c400000u) return 381;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0xcd778800u) == 0x4c718800u) return 88;
                                    } else {
                                        if ((word & 0xcd778800u) == 0x4c718800u) return 74;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd370800u) == 0x4c210800u) return 221;
                                        } else {
                                            if ((word & 0xcd370800u) == 0x4c210800u) return 217;
                                        }
                                    } else {
                                        if ((word & 0xcd370800u) == 0x4c210800u) return 293;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 448;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 446;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 447;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 445;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 456;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 454;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 455;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 453;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 450;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 444;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 449;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 443;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 458;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 452;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 457;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 451;
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00008000u) {
                        if (word & 0x02000000u) {
                            if (word & 0x01000000u) {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00001000u) {
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 672;
                                        } else {
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 666;
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 674;
                                        } else {
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 670;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0xcc001000u) == 0x4c001000u) return 772;
                                    } else {
                                        if ((word & 0xcc401000u) == 0x4c001000u) return 697;
                                    }
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0xccc01000u) == 0x4cc00000u) return 367;
                                        } else {
                                            if ((word & 0xcc001000u) == 0x4c000000u) return 709;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0xccc01000u) == 0x4cc00000u) return 555;
                                        } else {
                                            if ((word & 0xcc001000u) == 0x4c000000u) return 708;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if ((word & 0xccff0800u) == 0x4ce00800u) return 16;
                                    } else {
                                        if ((word & 0xccff0800u) == 0x4ce00800u) return 14;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 561;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 559;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 560;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 558;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 569;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 567;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 568;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 566;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 563;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 557;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 562;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 556;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 571;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 565;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 570;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 564;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00001000u) {
                            if (word & 0x00800000u) {
                                if (word & 0x00400000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x8f200800u) == 0x0e000000u) return 374;
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 432;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 430;
                                            }
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 431;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 429;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 434;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 428;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 433;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 427;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x8f000800u) == 0x0f000000u) return 661;
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 440;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 438;
                                            }
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 439;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 437;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 442;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 436;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 441;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 435;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 416;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 424;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 418;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 426;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x0f200000u) == 0x0e200000u) return 714;
                                        } else {
                                            if ((word & 0x0fc00000u) == 0x0f400000u) return 660;
                                        }
                                    } else {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 414;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 422;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 412;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 420;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 415;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 413;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 423;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 421;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 417;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 411;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 425;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 419;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            if (word & 0x00000400u) {
                if (word & 0x00008000u) {
                    if (word & 0x00002000u) {
                        if (word & 0x00000800u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00800000u) {
                                    if ((word & 0x8f604000u) == 0x0e404000u) return 579;
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e004000u) == 0x0e004000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 719;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e004000u) == 0x0e004000u) return 683;
                                        } else {
                                            if ((word & 0x8e404000u) == 0x0e404000u) return 578;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if (word & 0x00200000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 677;
                                        } else {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 676;
                                        }
                                    } else {
                                        if ((word & 0xcfc00000u) == 0x4e400000u) return 342;
                                    }
                                } else {
                                    if ((word & 0x8f200000u) == 0x0e200000u) return 830;
                                }
                            }
                        } else {
                            if (word & 0x01000000u) {
                                if (word & 0x00001000u) {
                                    if ((word & 0xcef84000u) == 0x4e004000u) return 313;
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0xcef80000u) == 0x4e000000u) return 314;
                                        if ((word & 0xcef80000u) == 0x0e000000u) return 315;
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 737;
                                    } else {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 743;
                                    }
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if (word & 0x00200000u) {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 690;
                                            } else {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 686;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 682;
                                            } else {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 681;
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 849;
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 805;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 828;
                                        } else {
                                            if ((word & 0xcec00000u) == 0x4e800000u) return 572;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00004000u) {
                            if (word & 0x00200000u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00001000u) {
                                        if ((word & 0x8f800000u) == 0x0e000000u) return 698;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f400000u) == 0x0e000000u) return 625;
                                        } else {
                                            if ((word & 0x8f400000u) == 0x0e000000u) return 622;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 675;
                                        } else {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 678;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 688;
                                        } else {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 684;
                                        }
                                    }
                                }
                            } else {
                                if ((word & 0x8f000000u) == 0x0e000000u) return 868;
                            }
                        } else {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 738;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 779;
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 726;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 751;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 804;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 740;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 776;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 825;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 730;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 815;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 803;
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x01000000u) {
                        if (word & 0x00001000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 739;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 742;
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 724;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 745;
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 727;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 741;
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 731;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 744;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 820;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 755;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 836;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 581;
                                                } else {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 583;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 582;
                                                } else {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 586;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 831;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 838;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 837;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 835;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 822;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 754;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 840;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 839;
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 829;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 827;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 842;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 826;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00800000u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if ((word & 0x8e404800u) == 0x0e400000u) return 620;
                                    } else {
                                        if ((word & 0x8e404800u) == 0x0e400000u) return 587;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8e406000u) == 0x0e402000u) return 591;
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e404000u) == 0x0e400000u) return 599;
                                        } else {
                                            if ((word & 0x8e404000u) == 0x0e400000u) return 618;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 613;
                                            } else {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 630;
                                            }
                                        } else {
                                            if ((word & 0x8e006000u) == 0x0e002000u) return 589;
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 616;
                                            } else {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 593;
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 597;
                                            } else {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 614;
                                            }
                                        }
                                    }
                                } else {
                                    if ((word & 0xce000000u) == 0x4e000000u) return 857;
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x00008000u) {
                    if (word & 0x00004000u) {
                        if (word & 0x00001000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 893;
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ef60800u) == 0x0ef00800u) return 248;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 288;
                                            } else {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 278;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ef60800u) == 0x0ef00800u) return 235;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xce660800u) == 0x4e200800u) return 118;
                                                } else {
                                                    if ((word & 0xce660800u) == 0x4e200800u) return 114;
                                                }
                                            } else {
                                                if ((word & 0x8ea60800u) == 0x0ea00800u) return 274;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 892;
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e760800u) == 0x0e700800u) return 246;
                                            } else {
                                                if ((word & 0x8e760800u) == 0x0e700800u) return 253;
                                            }
                                        } else {
                                            if ((word & 0x8ef60800u) == 0x0ef00800u) return 202;
                                        }
                                    } else {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 287;
                                            } else {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 295;
                                            }
                                        } else {
                                            if ((word & 0x8eb60800u) == 0x0ea00800u) return 260;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 895;
                                } else {
                                    if ((word & 0x8ebf0800u) == 0x0e210800u) return 276;
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8ec00000u) == 0x0e800000u) return 767;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8ef60000u) == 0x0e700000u) return 208;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 298;
                                                } else {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 263;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8ef60000u) == 0x0ef00000u) return 198;
                                            } else {
                                                if (word & 0x00100000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0xce660000u) == 0x4e200000u) return 116;
                                                    } else {
                                                        if ((word & 0xce660000u) == 0x4e200000u) return 112;
                                                    }
                                                } else {
                                                    if ((word & 0x8ea60000u) == 0x0ea00000u) return 258;
                                                }
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 834;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00001000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x00080000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f770800u) == 0x0e710800u) return 225;
                                    } else {
                                        if ((word & 0x8f770800u) == 0x0e710800u) return 212;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f360800u) == 0x0e200800u) return 273;
                                        } else {
                                            if ((word & 0x8f360800u) == 0x0e200800u) return 266;
                                        }
                                    } else {
                                        if ((word & 0x8f360800u) == 0x0e200800u) return 316;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 855;
                                    } else {
                                        if ((word & 0x8e400000u) == 0x0e000000u) return 771;
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8e770800u) == 0x0e710800u) return 240;
                                        } else {
                                            if ((word & 0x8e770800u) == 0x0e710800u) return 244;
                                        }
                                    } else {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 281;
                                            } else {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 285;
                                            }
                                        } else {
                                            if ((word & 0x8e360800u) == 0x0e200800u) return 307;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 898;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 220;
                                            } else {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 216;
                                            }
                                        } else {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00010000u) {
                                                    if ((word & 0x8e260000u) == 0x0e200000u) return 335;
                                                } else {
                                                    if ((word & 0x8e260000u) == 0x0e200000u) return 334;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e270000u) == 0x0e210000u) return 271;
                                                } else {
                                                    if ((word & 0x8e270000u) == 0x0e210000u) return 268;
                                                }
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 833;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8ec00000u) == 0x0e800000u) return 763;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8ef60000u) == 0x0e700000u) return 239;
                                            } else {
                                                if ((word & 0x8eb60000u) == 0x0e200000u) return 280;
                                            }
                                        } else {
                                            if ((word & 0x8e3e0000u) == 0x0e200000u) return 305;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 832;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x01000000u) {
                        if (word & 0x00001000u) {
                            if ((word & 0x8e000000u) == 0x0e000000u) return 900;
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 897;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 896;
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 882;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 881;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00004000u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e1f0000u) == 0x0e000000u) return 327;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x8e9f0000u) == 0x0e000000u) return 250;
                                            } else {
                                                if ((word & 0x8e9f0000u) == 0x0e000000u) return 249;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 821;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 819;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00010000u) {
                                            if ((word & 0x8e1e2000u) == 0x0e002000u) return 323;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x8e0e2000u) == 0x0e002000u) return 333;
                                            } else {
                                                if ((word & 0x8e0e2000u) == 0x0e002000u) return 337;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 844;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 824;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8ede0000u) == 0x0e400000u) return 222;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 331;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 781;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 329;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 332;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 843;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 336;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 303;
                                            }
                                        } else {
                                            if ((word & 0x8e1f0000u) == 0x0e000000u) return 318;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 780;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 823;
                                        }
                                    }
                                }
                            }
                        } else {
                            if ((word & 0x8ec00000u) == 0x0e000000u) return 867;
                        }
                    }
                }
            }
        }
    } else {
        if (word & 0x10000000u) {
            if (word & 0x40000000u) {
                if (word & 0x00002000u) {
                    if (word & 0x00008000u) {
                        if (word & 0x00004000u) {
                            if (word & 0x00000400u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f201000u) == 0x0e201000u) return 634;
                                    } else {
                                        if (word & 0x01000000u) {
                                            if ((word & 0x8e001000u) == 0x0e001000u && (((word >> 19) & 15u) != 0x0u)) return 662;
                                        } else {
                                            if ((word & 0x8e201000u) == 0x0e201000u) return 632;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e801000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u)) return 664;
                                    } else {
                                        if ((word & 0x8ea01000u) == 0x0e200000u) return 596;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ff60800u) == 0x0ef00800u) return 136;
                                        } else {
                                            if ((word & 0x8fb60800u) == 0x0ea00800u) return 238;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f7e0800u) == 0x0e300800u) return 117;
                                        } else {
                                            if ((word & 0x8f7e0800u) == 0x0e300800u) return 113;
                                        }
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if ((word & 0x8ff70800u) == 0x0ef00800u) return 26;
                                    } else {
                                        if ((word & 0x8fb70800u) == 0x0ea00800u) return 205;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00001000u) {
                                if (word & 0x00000400u) {
                                    if ((word & 0x8f200800u) == 0x0e200000u) return 706;
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 861;
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e670000u) == 0x0e610000u) return 96;
                                                    } else {
                                                        if ((word & 0x8e670000u) == 0x0e610000u) return 53;
                                                    }
                                                } else {
                                                    if ((word & 0x8ee70000u) == 0x0ee10000u) return 7;
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e2e0000u) == 0x0e200000u) return 224;
                                                    } else {
                                                        if ((word & 0x8e2e0000u) == 0x0e200000u) return 211;
                                                    }
                                                } else {
                                                    if ((word & 0x8eee0000u) == 0x0ee00000u) return 6;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e200000u) == 0x0e200000u) return 705;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f770c00u) == 0x0e710800u) return 81;
                                    } else {
                                        if ((word & 0x8f770c00u) == 0x0e710800u) return 67;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f360c00u) == 0x0e200800u) return 219;
                                        } else {
                                            if ((word & 0x8f360c00u) == 0x0e200800u) return 215;
                                        }
                                    } else {
                                        if ((word & 0x8ff60c00u) == 0x0ee00800u) return 17;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00000400u) {
                            if (word & 0x00000800u) {
                                if (word & 0x00200000u) {
                                    if (word & 0x00001000u) {
                                        if ((word & 0x8fc04000u) == 0x0ec00000u) return 368;
                                    } else {
                                        if ((word & 0x8f004000u) == 0x0e000000u) return 713;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f405000u) == 0x0e401000u) return 406;
                                    } else {
                                        if ((word & 0x8f405000u) == 0x0e401000u) return 405;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x8f800000u) == 0x0f000000u && (((word >> 19) & 15u) != 0x0u)) return 668;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f600000u) == 0x0e600000u) return 369;
                                        } else {
                                            if ((word & 0x8f400000u) == 0x0f400000u) return 648;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8ec04000u) == 0x0e400000u) return 647;
                                    } else {
                                        if ((word & 0x8ee04000u) == 0x0e400000u) return 380;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00001000u) {
                                if (word & 0x00004000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 859;
                                    } else {
                                        if ((word & 0x8e3f0800u) == 0x0e200800u) return 290;
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 858;
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x8e3f0000u) == 0x0e200000u) return 294;
                                        } else {
                                            if ((word & 0x8ee00000u) == 0x0e000000u) return 543;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00000800u) {
                                    if ((word & 0x8fff4000u) == 0x0e280000u) return 184;
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x8fe00000u) == 0x0e000000u) return 546;
                                    } else {
                                        if ((word & 0x8fe00000u) == 0x0e000000u) return 541;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00008000u) {
                        if (word & 0x00001000u) {
                            if (word & 0x00004000u) {
                                if (word & 0x00000400u) {
                                    if ((word & 0x8fa00800u) == 0x0e200800u) return 629;
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 864;
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e760000u) == 0x0e700000u) return 135;
                                                    } else {
                                                        if ((word & 0x8e760000u) == 0x0e700000u) return 181;
                                                    }
                                                } else {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e360000u) == 0x0e200000u) return 237;
                                                    } else {
                                                        if ((word & 0x8e360000u) == 0x0e200000u) return 252;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00080000u) {
                                                    if ((word & 0x8ef60000u) == 0x0ef00000u) return 22;
                                                } else {
                                                    if (word & 0x00100000u) {
                                                        if ((word & 0x8ee60000u) == 0x0e200000u) return 21;
                                                    } else {
                                                        if ((word & 0x8ea60000u) == 0x0ea00000u) return 197;
                                                    }
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e200000u) == 0x0e200000u) return 707;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8f800000u) == 0x0f000000u && (((word >> 19) & 15u) != 0x0u)) return 665;
                                    } else {
                                        if ((word & 0x8f800000u) == 0x0f000000u && (((word >> 19) & 15u) != 0x0u)) return 669;
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 774;
                                        } else {
                                            if ((word & 0x8e400000u) == 0x0e000000u) return 699;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x8eff0000u) == 0x0ee00000u) return 13;
                                        } else {
                                            if ((word & 0x8e200000u) == 0x0e200000u) return 704;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00004000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000400u) == 0x0e000000u) return 860;
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8e770c00u) == 0x0e700800u) return 24;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x8e670c00u) == 0x0e200800u) return 115;
                                            } else {
                                                if ((word & 0x8e270c00u) == 0x0e200800u) return 201;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8e760c00u) == 0x0e700800u) return 39;
                                            } else {
                                                if ((word & 0x8e360c00u) == 0x0e200800u) return 207;
                                            }
                                        } else {
                                            if ((word & 0x8e7e0c00u) == 0x0e300800u) return 111;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8fe00000u) == 0x0ee00000u) return 372;
                                    } else {
                                        if ((word & 0x8fe00000u) == 0x0ee00000u) return 341;
                                    }
                                } else {
                                    if ((word & 0x8fff0800u) == 0x0ee00800u) return 15;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00000400u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x8f200000u) == 0x0e200000u) return 711;
                                    } else {
                                        if ((word & 0x8fe00000u) == 0x0e400000u) return 398;
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f600000u) == 0x0e600000u) return 553;
                                        } else {
                                            if ((word & 0x8f400000u) == 0x0f400000u) return 644;
                                        }
                                    } else {
                                        if ((word & 0x8fc00000u) == 0x0f400000u) return 650;
                                    }
                                }
                            } else {
                                if (word & 0x00000800u) {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x8f200000u) == 0x0e200000u) return 712;
                                    } else {
                                        if ((word & 0x8f200000u) == 0x0e200000u) return 703;
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x8fe00000u) == 0x0ee00000u) return 554;
                                    } else {
                                        if (word & 0x00400000u) {
                                            if ((word & 0x8f800000u) == 0x0f000000u) return 649;
                                        } else {
                                            if ((word & 0x8fa00000u) == 0x0e000000u) return 373;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00004000u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f000000u) == 0x0f000000u) return 770;
                                    } else {
                                        if (word & 0x01000000u) {
                                            if ((word & 0x8e400000u) == 0x0e000000u) return 695;
                                        } else {
                                            if ((word & 0x8e600800u) == 0x0e000000u) return 544;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8f3f0000u) == 0x0e210000u) return 292;
                                    } else {
                                        if ((word & 0x8fe00000u) == 0x0e000000u) return 545;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x01000000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 766;
                                        } else {
                                            if ((word & 0x8e400000u) == 0x0e000000u) return 693;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x8eff0000u) == 0x0e280000u) return 183;
                                        } else {
                                            if ((word & 0x8ee00000u) == 0x0e000000u) return 542;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8fff0000u) == 0x0e280000u) return 182;
                                    } else {
                                        if ((word & 0x8fe00000u) == 0x0e000000u) return 540;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x00400000u) {
                    if (word & 0x00800000u) {
                        if (word & 0x80000000u) {
                            if (word & 0x00010000u) {
                                if (word & 0x00020000u) {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f38fc00u) == 0x0e200000u) return 127;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f18fc00u) == 0x0e000000u) return 189;
                                        } else {
                                            if ((word & 0x0f180000u) == 0x0e000000u) return 654;
                                        }
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x0f04fc00u) == 0x0e000000u) return 101;
                                            } else {
                                                if ((word & 0x0f040000u) == 0x0e000000u) return 611;
                                            }
                                        } else {
                                            if ((word & 0x0f24fc00u) == 0x0e200000u) return 86;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 44;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 58;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 72;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00020000u) {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f38fc00u) == 0x0e200000u) return 122;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f18fc00u) == 0x0e000000u) return 178;
                                        } else {
                                            if ((word & 0x0f180000u) == 0x0e000000u) return 641;
                                        }
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x0f04fc00u) == 0x0e000000u) return 94;
                                            } else {
                                                if ((word & 0x0f040000u) == 0x0e000000u) return 605;
                                            }
                                        } else {
                                            if ((word & 0x0f24fc00u) == 0x0e200000u) return 79;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 37;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 51;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 65;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00200000u) {
                                if (word & 0x01000000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 879;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 876;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x0e000000u) == 0x0e000000u) return 851;
                                        } else {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 393;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 408;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 396;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 384;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 387;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 378;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 390;
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 403;
                                                        } else {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 400;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000010u) {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 758;
                                            } else {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 761;
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x0e0003e0u) == 0x0e000000u) return 311;
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e1c2000u) == 0x0e040000u) return 149;
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 164;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 168;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e1c2000u) == 0x0e000000u) return 104;
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 158;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 19;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e1c2000u) == 0x0e040000u) return 161;
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 152;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 133;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 146;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 109;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 155;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 128;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00002000u) {
                                                        if (word & 0x00000008u) {
                                                            if (word & 0x00000010u) {
                                                                if ((word & 0x0e1f8007u) == 0x0e000000u) return 1;
                                                            } else {
                                                                if ((word & 0x0e1f8007u) == 0x0e000000u) return 4;
                                                            }
                                                        } else {
                                                            if (word & 0x00000010u) {
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 28;
                                                            } else {
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 31;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 126;
                                                                } else {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 188;
                                                                }
                                                            } else {
                                                                if (word & 0x00080000u) {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 98;
                                                                    } else {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 83;
                                                                    }
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 41;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 55;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 69;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 119;
                                                                } else {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 177;
                                                                }
                                                            } else {
                                                                if (word & 0x00080000u) {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 91;
                                                                    } else {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 76;
                                                                    }
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 34;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 48;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 62;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 873;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 870;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 653;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 608;
                                        }
                                    } else {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 640;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 602;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x80000000u) {
                            if (word & 0x00010000u) {
                                if (word & 0x00020000u) {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f38fc00u) == 0x0e200000u) return 124;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f18fc00u) == 0x0e000000u) return 187;
                                        } else {
                                            if ((word & 0x0f180000u) == 0x0e000000u) return 652;
                                        }
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x0f04fc00u) == 0x0e000000u) return 100;
                                            } else {
                                                if ((word & 0x0f040000u) == 0x0e000000u) return 610;
                                            }
                                        } else {
                                            if ((word & 0x0f24fc00u) == 0x0e200000u) return 85;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 43;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 57;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 71;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00020000u) {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f38fc00u) == 0x0e200000u) return 121;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f18fc00u) == 0x0e000000u) return 176;
                                        } else {
                                            if ((word & 0x0f180000u) == 0x0e000000u) return 639;
                                        }
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x0f04fc00u) == 0x0e000000u) return 93;
                                            } else {
                                                if ((word & 0x0f040000u) == 0x0e000000u) return 604;
                                            }
                                        } else {
                                            if ((word & 0x0f24fc00u) == 0x0e200000u) return 78;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 36;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 50;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 64;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00200000u) {
                                if (word & 0x01000000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 878;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 875;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x0e000000u) == 0x0e000000u) return 850;
                                        } else {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 392;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 407;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 395;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 383;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 386;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 377;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 389;
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 402;
                                                        } else {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 399;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000010u) {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 757;
                                            } else {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 760;
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x0e0003e0u) == 0x0e000000u) return 310;
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 148;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 106;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 163;
                                                                } else {
                                                                    if (word & 0x00080000u) {
                                                                        if ((word & 0x0e102000u) == 0x0e000000u) return 141;
                                                                    } else {
                                                                        if ((word & 0x0e102000u) == 0x0e000000u) return 167;
                                                                    }
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e1a2000u) == 0x0e000000u) return 157;
                                                            } else {
                                                                if (word & 0x00080000u) {
                                                                    if ((word & 0x0e122000u) == 0x0e000000u) return 137;
                                                                } else {
                                                                    if ((word & 0x0e122000u) == 0x0e000000u) return 18;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 160;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 12;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 151;
                                                                } else {
                                                                    if (word & 0x00080000u) {
                                                                        if ((word & 0x0e102000u) == 0x0e000000u) return 143;
                                                                    } else {
                                                                        if ((word & 0x0e102000u) == 0x0e000000u) return 132;
                                                                    }
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 145;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 108;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 154;
                                                                } else {
                                                                    if (word & 0x00080000u) {
                                                                        if ((word & 0x0e102000u) == 0x0e000000u) return 139;
                                                                    } else {
                                                                        if ((word & 0x0e102000u) == 0x0e000000u) return 125;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00002000u) {
                                                        if (word & 0x00000008u) {
                                                            if (word & 0x00000010u) {
                                                                if ((word & 0x0e1f8007u) == 0x0e000000u) return 0;
                                                            } else {
                                                                if ((word & 0x0e1f8007u) == 0x0e000000u) return 3;
                                                            }
                                                        } else {
                                                            if (word & 0x00000010u) {
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 27;
                                                            } else {
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 30;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00100000u) {
                                                                    if ((word & 0x0e068000u) == 0x0e000000u) return 97;
                                                                } else {
                                                                    if ((word & 0x0e068000u) == 0x0e000000u) return 82;
                                                                }
                                                            } else {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e148000u) == 0x0e000000u) return 186;
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 40;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 54;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 68;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e148000u) == 0x0e140000u) return 110;
                                                                } else {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 90;
                                                                    } else {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 75;
                                                                    }
                                                                }
                                                            } else {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e148000u) == 0x0e000000u) return 175;
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 33;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 47;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 61;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 872;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 869;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 651;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 607;
                                        }
                                    } else {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 638;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 601;
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x80000000u) {
                        if (word & 0x00010000u) {
                            if (word & 0x00080000u) {
                                if (word & 0x00100000u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x0f86fc00u) == 0x0e000000u) return 102;
                                    } else {
                                        if ((word & 0x0f860000u) == 0x0e000000u) return 612;
                                    }
                                } else {
                                    if (word & 0x00020000u) {
                                        if ((word & 0x0fa4fc00u) == 0x0ea40000u) return 131;
                                    } else {
                                        if ((word & 0x0fa4fc00u) == 0x0e200000u) return 87;
                                    }
                                }
                            } else {
                                if (word & 0x00020000u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x0f94fc00u) == 0x0e000000u) return 191;
                                    } else {
                                        if ((word & 0x0f940000u) == 0x0e000000u) return 656;
                                    }
                                } else {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0fb0fc00u) == 0x0e200000u) return 45;
                                    } else {
                                        if (word & 0x00100000u) {
                                            if ((word & 0x0fa0fc00u) == 0x0e200000u) return 59;
                                        } else {
                                            if ((word & 0x0fa0fc00u) == 0x0e200000u) return 73;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00080000u) {
                                if (word & 0x00100000u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x0f86fc00u) == 0x0e000000u) return 95;
                                    } else {
                                        if ((word & 0x0f860000u) == 0x0e000000u) return 606;
                                    }
                                } else {
                                    if (word & 0x00020000u) {
                                        if ((word & 0x0fa4fc00u) == 0x0ea40000u) return 123;
                                    } else {
                                        if ((word & 0x0fa4fc00u) == 0x0e200000u) return 80;
                                    }
                                }
                            } else {
                                if (word & 0x00020000u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x0f94fc00u) == 0x0e000000u) return 180;
                                    } else {
                                        if ((word & 0x0f940000u) == 0x0e000000u) return 643;
                                    }
                                } else {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0fb0fc00u) == 0x0e200000u) return 38;
                                    } else {
                                        if (word & 0x00100000u) {
                                            if ((word & 0x0fa0fc00u) == 0x0e200000u) return 52;
                                        } else {
                                            if ((word & 0x0fa0fc00u) == 0x0e200000u) return 66;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x01000000u) {
                                if (word & 0x00008000u) {
                                    if ((word & 0x0e800000u) == 0x0e000000u) return 880;
                                } else {
                                    if ((word & 0x0e800000u) == 0x0e000000u) return 877;
                                }
                            } else {
                                if (word & 0x00000800u) {
                                    if (word & 0x00000400u) {
                                        if ((word & 0x0e800000u) == 0x0e000000u) return 852;
                                    } else {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e808000u) == 0x0e000000u) return 394;
                                                } else {
                                                    if ((word & 0x0e808000u) == 0x0e000000u) return 409;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e808000u) == 0x0e000000u) return 397;
                                                } else {
                                                    if ((word & 0x0e808000u) == 0x0e000000u) return 385;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e808000u) == 0x0e000000u) return 388;
                                                } else {
                                                    if ((word & 0x0e808000u) == 0x0e000000u) return 379;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e808000u) == 0x0e000000u) return 391;
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x0e800000u) == 0x0e000000u) return 404;
                                                    } else {
                                                        if ((word & 0x0e800000u) == 0x0e000000u) return 401;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000010u) {
                                            if ((word & 0x0e800000u) == 0x0e000000u) return 759;
                                        } else {
                                            if ((word & 0x0e800000u) == 0x0e000000u) return 762;
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if ((word & 0x0e8003e0u) == 0x0e000000u) return 312;
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00020000u) {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 150;
                                                            } else {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 107;
                                                            }
                                                        } else {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 165;
                                                            } else {
                                                                if (word & 0x00080000u) {
                                                                    if ((word & 0x0e902000u) == 0x0e000000u) return 142;
                                                                } else {
                                                                    if ((word & 0x0e902000u) == 0x0e000000u) return 169;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00020000u) {
                                                            if ((word & 0x0e9c2000u) == 0x0e000000u) return 105;
                                                        } else {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 159;
                                                            } else {
                                                                if (word & 0x00080000u) {
                                                                    if ((word & 0x0e902000u) == 0x0e000000u) return 138;
                                                                } else {
                                                                    if ((word & 0x0e902000u) == 0x0e000000u) return 20;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00040000u) {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 162;
                                                            } else {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 153;
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x0e922000u) == 0x0e000000u) return 144;
                                                            } else {
                                                                if ((word & 0x0e922000u) == 0x0e000000u) return 134;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00040000u) {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 147;
                                                            } else {
                                                                if ((word & 0x0e982000u) == 0x0e000000u) return 156;
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x0e922000u) == 0x0e000000u) return 140;
                                                            } else {
                                                                if ((word & 0x0e922000u) == 0x0e000000u) return 130;
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00000008u) {
                                                        if (word & 0x00000010u) {
                                                            if ((word & 0x0e9f8007u) == 0x0e000000u) return 2;
                                                        } else {
                                                            if ((word & 0x0e9f8007u) == 0x0e000000u) return 5;
                                                        }
                                                    } else {
                                                        if (word & 0x00000010u) {
                                                            if ((word & 0x0e808007u) == 0x0e000000u) return 29;
                                                        } else {
                                                            if ((word & 0x0e808007u) == 0x0e000000u) return 32;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00020000u) {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e988000u) == 0x0e000000u) return 129;
                                                            } else {
                                                                if ((word & 0x0e988000u) == 0x0e000000u) return 190;
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00100000u) {
                                                                    if ((word & 0x0e848000u) == 0x0e000000u) return 99;
                                                                } else {
                                                                    if ((word & 0x0e848000u) == 0x0e000000u) return 84;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e908000u) == 0x0e000000u) return 42;
                                                                } else {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e808000u) == 0x0e000000u) return 56;
                                                                    } else {
                                                                        if ((word & 0x0e808000u) == 0x0e000000u) return 70;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00020000u) {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e988000u) == 0x0e000000u) return 120;
                                                            } else {
                                                                if ((word & 0x0e988000u) == 0x0e000000u) return 179;
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00100000u) {
                                                                    if ((word & 0x0e848000u) == 0x0e000000u) return 92;
                                                                } else {
                                                                    if ((word & 0x0e848000u) == 0x0e000000u) return 77;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e908000u) == 0x0e000000u) return 35;
                                                                } else {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e808000u) == 0x0e000000u) return 49;
                                                                    } else {
                                                                        if ((word & 0x0e808000u) == 0x0e000000u) return 63;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x01000000u) {
                                if (word & 0x00008000u) {
                                    if ((word & 0x0e800000u) == 0x0e000000u) return 874;
                                } else {
                                    if ((word & 0x0e800000u) == 0x0e000000u) return 871;
                                }
                            } else {
                                if (word & 0x00010000u) {
                                    if (word & 0x00020000u) {
                                        if ((word & 0x0e9c0000u) == 0x0e000000u) return 655;
                                    } else {
                                        if ((word & 0x0e9c0000u) == 0x0e180000u) return 609;
                                    }
                                } else {
                                    if (word & 0x00020000u) {
                                        if ((word & 0x0e9c0000u) == 0x0e000000u) return 642;
                                    } else {
                                        if ((word & 0x0e9c0000u) == 0x0e180000u) return 603;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            if (word & 0x01000000u) {
                if (word & 0x00002000u) {
                    if (word & 0x00000400u) {
                        if (word & 0x00001000u) {
                            if (word & 0x00000800u) {
                                if ((word & 0x8ef8c000u) == 0x0e00c000u) return 339;
                                if ((word & 0x8e80c000u) == 0x0e00c000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 718;
                            } else {
                                if (word & 0x00004000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8ef80000u) == 0x0e000000u) return 340;
                                    } else {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 728;
                                    }
                                } else {
                                    if ((word & 0x8e808000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 733;
                                }
                            }
                        } else {
                            if (word & 0x00004000u) {
                                if ((word & 0x8e808800u) == 0x0e008000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 721;
                            } else {
                                if (word & 0x00008000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 734;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 732;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00001000u) {
                            if (word & 0x00004000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 750;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 749;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 841;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 816;
                                        }
                                    }
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 889;
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 891;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 888;
                                }
                            }
                        } else {
                            if (word & 0x00004000u) {
                                if (word & 0x00008000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 884;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 886;
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 887;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 885;
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00008000u) {
                        if (word & 0x00000400u) {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if ((word & 0x8e804000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 725;
                                } else {
                                    if ((word & 0x8e804000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 720;
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if ((word & 0x8e804000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 729;
                                } else {
                                    if ((word & 0x8e804000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 723;
                                }
                            }
                        } else {
                            if (word & 0x00001000u) {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 894;
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 856;
                                    } else {
                                        if ((word & 0x8e400000u) == 0x0e000000u) return 773;
                                    }
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 890;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 883;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00004000u) {
                            if (word & 0x00800000u) {
                                if (word & 0x00001000u) {
                                    if ((word & 0x8e000400u) == 0x0e000000u) return 854;
                                } else {
                                    if ((word & 0x8e400400u) == 0x0e000000u) return 768;
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if ((word & 0x8e001800u) == 0x0e001000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 722;
                                } else {
                                    if ((word & 0x8e401000u) == 0x0e001000u) return 769;
                                }
                            }
                        } else {
                            if (word & 0x00000400u) {
                                if (word & 0x00001000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 736;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 735;
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 853;
                                    } else {
                                        if ((word & 0x8e400000u) == 0x0e000000u) return 765;
                                    }
                                } else {
                                    if ((word & 0x8ec00000u) == 0x0e800000u) return 764;
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x00200000u) {
                    if (word & 0x00008000u) {
                        if (word & 0x00002000u) {
                            if (word & 0x00000400u) {
                                if (word & 0x02000000u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8c000000u) == 0x0c000000u) return 701;
                                                } else {
                                                    if ((word & 0x8c000000u) == 0x0c000000u) return 700;
                                                }
                                            } else {
                                                if ((word & 0x8c000000u) == 0x0c000000u) return 747;
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8c400000u) == 0x0c000000u) return 626;
                                                } else {
                                                    if ((word & 0x8c400000u) == 0x0c000000u) return 623;
                                                }
                                            } else {
                                                if ((word & 0x8c000000u) == 0x0c000000u) return 793;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8c000000u) == 0x0c000000u) return 691;
                                                } else {
                                                    if ((word & 0x8c000000u) == 0x0c000000u) return 687;
                                                }
                                            } else {
                                                if ((word & 0x8c000000u) == 0x0c000000u) return 801;
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x8c800000u) == 0x0c000000u) return 680;
                                            } else {
                                                if ((word & 0x8c000000u) == 0x0c000000u) return 791;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x40000000u) {
                                            if (word & 0x00800000u) {
                                                if (word & 0x80000000u) {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 348;
                                                } else {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 346;
                                                }
                                            } else {
                                                if ((word & 0x8c005800u) == 0x08005800u) return 358;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if (word & 0x80000000u) {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 347;
                                                } else {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 345;
                                                }
                                            } else {
                                                if ((word & 0x8c005800u) == 0x08005800u) return 357;
                                            }
                                        }
                                    } else {
                                        if (word & 0x40000000u) {
                                            if (word & 0x00800000u) {
                                                if (word & 0x80000000u) {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 356;
                                                } else {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 354;
                                                }
                                            } else {
                                                if ((word & 0x8c005800u) == 0x08005800u) return 362;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if (word & 0x80000000u) {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 355;
                                                } else {
                                                    if ((word & 0x0c005800u) == 0x08005800u) return 353;
                                                }
                                            } else {
                                                if ((word & 0x8c005800u) == 0x08005800u) return 361;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8ec70800u) == 0x0ec00800u) return 194;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e470800u) == 0x0e000800u) return 234;
                                                } else {
                                                    if ((word & 0x8e470800u) == 0x0e000800u) return 230;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e8e0800u) == 0x0e000800u) return 279;
                                            } else {
                                                if ((word & 0x8e8e0800u) == 0x0e800800u) return 256;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e470000u) == 0x0e410000u) return 223;
                                                    } else {
                                                        if ((word & 0x8e470000u) == 0x0e410000u) return 210;
                                                    }
                                                } else {
                                                    if ((word & 0x8e070000u) == 0x0e010000u) return 301;
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e0e0000u) == 0x0e000000u) return 272;
                                                    } else {
                                                        if ((word & 0x8e0e0000u) == 0x0e000000u) return 265;
                                                    }
                                                } else {
                                                    if ((word & 0x8e0e0000u) == 0x0e000000u) return 300;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 800;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e9e0000u) == 0x0e000000u) return 277;
                                            } else {
                                                if (word & 0x00080000u) {
                                                    if ((word & 0x8ed60000u) == 0x0ed00000u) return 204;
                                                } else {
                                                    if ((word & 0x8e960000u) == 0x0e800000u) return 261;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 778;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e470000u) == 0x0e410000u) return 218;
                                                    } else {
                                                        if ((word & 0x8e470000u) == 0x0e410000u) return 214;
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x8e060000u) == 0x0e000000u) return 325;
                                                    } else {
                                                        if ((word & 0x8e060000u) == 0x0e000000u) return 324;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e0e0000u) == 0x0e000000u) return 270;
                                                    } else {
                                                        if ((word & 0x8e0e0000u) == 0x0e000000u) return 267;
                                                    }
                                                } else {
                                                    if ((word & 0x8e0e0000u) == 0x0e000000u) return 308;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 796;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00004000u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x8e800000u) == 0x0e000000u) return 696;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 702;
                                            } else {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 679;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e560000u) == 0x0e500000u) return 236;
                                                    } else {
                                                        if ((word & 0x8e560000u) == 0x0e500000u) return 251;
                                                    }
                                                } else {
                                                    if ((word & 0x8ed60000u) == 0x0ed00000u) return 196;
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e160000u) == 0x0e000000u) return 275;
                                                    } else {
                                                        if ((word & 0x8e160000u) == 0x0e000000u) return 289;
                                                    }
                                                } else {
                                                    if ((word & 0x8e960000u) == 0x0e800000u) return 257;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 802;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 694;
                                            } else {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 692;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 689;
                                            } else {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 685;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00080000u) {
                                                    if ((word & 0x8ed60000u) == 0x0e500000u) return 206;
                                                } else {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e160000u) == 0x0e000000u) return 297;
                                                    } else {
                                                        if ((word & 0x8e160000u) == 0x0e000000u) return 262;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00080000u) {
                                                    if ((word & 0x8ed60000u) == 0x0ed00000u) return 200;
                                                } else {
                                                    if (word & 0x00100000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x8e460000u) == 0x0e000000u) return 232;
                                                        } else {
                                                            if ((word & 0x8e460000u) == 0x0e000000u) return 228;
                                                        }
                                                    } else {
                                                        if ((word & 0x8e860000u) == 0x0e800000u) return 259;
                                                    }
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 797;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 777;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 775;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e570000u) == 0x0e510000u) return 245;
                                                } else {
                                                    if ((word & 0x8e570000u) == 0x0e510000u) return 241;
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e160000u) == 0x0e000000u) return 286;
                                                    } else {
                                                        if ((word & 0x8e160000u) == 0x0e000000u) return 282;
                                                    }
                                                } else {
                                                    if ((word & 0x8e160000u) == 0x0e000000u) return 304;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 799;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x4ec00000u) == 0x4e400000u) return 539;
                                            } else {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 756;
                                            }
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x4ec00000u) == 0x4e400000u) return 547;
                                            } else {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 748;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x4ec00000u) == 0x4e400000u) return 549;
                                            } else {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x0e570000u) == 0x0e510000u) return 243;
                                                    } else {
                                                        if ((word & 0x0e570000u) == 0x0e510000u) return 242;
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0e160000u) == 0x0e000000u) return 284;
                                                        } else {
                                                            if ((word & 0x0e160000u) == 0x0e000000u) return 283;
                                                        }
                                                    } else {
                                                        if ((word & 0x0e160000u) == 0x0e000000u) return 306;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x4ec00000u) == 0x4e400000u) return 548;
                                            } else {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 795;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x02000000u) {
                            if (word & 0x80000000u) {
                                if ((word & 0x4cc00000u) == 0x4c000000u) return 865;
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 783;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 752;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 806;
                                                } else {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0c000000u) == 0x0c000000u) return 636;
                                                        } else {
                                                            if ((word & 0x0c000000u) == 0x0c000000u) return 580;
                                                        }
                                                    } else {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0c000000u) == 0x0c000000u) return 637;
                                                        } else {
                                                            if ((word & 0x0c000000u) == 0x0c000000u) return 577;
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 785;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 753;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 810;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 809;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x0c9e0000u) == 0x0c000000u) return 264;
                                                    } else {
                                                        if (word & 0x00080000u) {
                                                            if ((word & 0x4cd60000u) == 0x4c000000u) return 10;
                                                        } else {
                                                            if ((word & 0x0c160000u) == 0x0c000000u) return 326;
                                                        }
                                                    }
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 784;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00100000u) {
                                                        if ((word & 0x0c0f0000u) == 0x0c000000u) return 322;
                                                    } else {
                                                        if ((word & 0x0c0f0000u) == 0x0c000000u) return 330;
                                                    }
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 813;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00004000u) {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x4cd70000u) == 0x4c000000u) return 8;
                                                    } else {
                                                        if ((word & 0x0c170000u) == 0x0c000000u) return 309;
                                                    }
                                                } else {
                                                    if ((word & 0x0c1f0000u) == 0x0c000000u) return 317;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 782;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 787;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 794;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 792;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0c5e0000u) == 0x0c000000u) return 193;
                                                        } else {
                                                            if ((word & 0x0c1e0000u) == 0x0c000000u) return 269;
                                                        }
                                                    } else {
                                                        if (word & 0x00080000u) {
                                                            if ((word & 0x4cd60000u) == 0x4c000000u) return 11;
                                                        } else {
                                                            if ((word & 0x0c160000u) == 0x0c000000u) return 320;
                                                        }
                                                    }
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 814;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 808;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 790;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x0c1e0000u) == 0x0c000000u) return 338;
                                                    } else {
                                                        if ((word & 0x0c1e0000u) == 0x0c000000u) return 321;
                                                    }
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 812;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 807;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 798;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 811;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 789;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00004000u) {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x0c1e0000u) == 0x0c000000u) return 328;
                                                    } else {
                                                        if (word & 0x00080000u) {
                                                            if ((word & 0x4cd60000u) == 0x4c000000u) return 9;
                                                        } else {
                                                            if ((word & 0x0c160000u) == 0x0c000000u) return 302;
                                                        }
                                                    }
                                                } else {
                                                    if ((word & 0x0c1f0000u) == 0x0c000000u) return 319;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 746;
                                                } else {
                                                    if ((word & 0x0c000000u) == 0x0c000000u) return 786;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 350;
                                        } else {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 344;
                                        }
                                    } else {
                                        if ((word & 0x8c007c00u) == 0x08007c00u) return 360;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 349;
                                        } else {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 343;
                                        }
                                    } else {
                                        if ((word & 0x8c007c00u) == 0x08007c00u) return 359;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 366;
                                        } else {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 352;
                                        }
                                    } else {
                                        if ((word & 0x8c007c00u) == 0x08007c00u) return 364;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 365;
                                        } else {
                                            if ((word & 0x0c007c00u) == 0x08007c00u) return 351;
                                        }
                                    } else {
                                        if ((word & 0x8c007c00u) == 0x08007c00u) return 363;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x80000000u) {
                        if (word & 0x00400000u) {
                            if ((word & 0x4e9ffc00u) == 0x4e808000u) return 185;
                        } else {
                            if (word & 0x00800000u) {
                                if ((word & 0x4e000000u) == 0x4e000000u) return 899;
                            } else {
                                if ((word & 0x4e008000u) == 0x4e000000u) return 866;
                            }
                        }
                    } else {
                        if (word & 0x00002000u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x0e00c000u) == 0x0e000000u) return 633;
                                            } else {
                                                if ((word & 0x0e00c000u) == 0x0e000000u) return 631;
                                            }
                                        } else {
                                            if (word & 0x40000000u) {
                                                if ((word & 0x0e8fc000u) == 0x0e080000u) return 255;
                                            } else {
                                                if ((word & 0x0e80c000u) == 0x0e000000u) return 573;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0e40c000u) == 0x0e400000u) return 621;
                                        } else {
                                            if ((word & 0x0e40c000u) == 0x0e400000u) return 617;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x0e008800u) == 0x0e000800u) return 848;
                                    } else {
                                        if ((word & 0x0e008800u) == 0x0e000800u) return 847;
                                    }
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x4ec04000u) == 0x4e800000u) return 576;
                                        } else {
                                            if (word & 0x40000000u) {
                                                if ((word & 0x0ec04000u) == 0x0e000000u) return 552;
                                            } else {
                                                if ((word & 0x0ec04000u) == 0x0e000000u) return 551;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x4ec04000u) == 0x4e800000u) return 550;
                                        } else {
                                            if ((word & 0x0ec04000u) == 0x0e400000u) return 595;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x0e008800u) == 0x0e000800u) return 818;
                                    } else {
                                        if ((word & 0x0e008800u) == 0x0e000800u) return 817;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x0ec04000u) == 0x0e800000u) return 659;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x0e804000u) == 0x0e000000u) return 628;
                                            } else {
                                                if ((word & 0x4e804000u) == 0x4e000000u) return 410;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x0e008000u) == 0x0e000000u) return 846;
                                        } else {
                                            if ((word & 0x0e008000u) == 0x0e000000u) return 845;
                                        }
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0e00c400u) == 0x0e000400u) return 627;
                                        } else {
                                            if ((word & 0x0e00c400u) == 0x0e000400u) return 624;
                                        }
                                    } else {
                                        if ((word & 0x0e80c400u) == 0x0e000400u) return 584;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x0e004400u) == 0x0e000400u) return 788;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0e404400u) == 0x0e400400u) return 635;
                                        } else {
                                            if ((word & 0x0e404400u) == 0x0e400400u) return 594;
                                        }
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0e00c400u) == 0x0e000400u) return 619;
                                        } else {
                                            if ((word & 0x0e00c400u) == 0x0e000400u) return 615;
                                        }
                                    } else {
                                        if ((word & 0x0e80c400u) == 0x0e000400u) return 585;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return -1;
}
