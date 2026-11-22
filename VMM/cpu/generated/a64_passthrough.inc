enum : uint32_t { CPU_PT_STUB_COUNT = 19950 };

static const char *const cpu_pt_feature_names[] = {
    "FEAT_DotProd",
    "FEAT_FCMA",
    "FEAT_FHM",
    "FEAT_FP16",
    "FEAT_JSCVT",
    "FEAT_LRCPC",
    "FEAT_LSE",
    "FEAT_RDM",
};

enum : uint32_t {
    CPU_PT_FEAT_DotProd = 1u << 0,
    CPU_PT_FEAT_FCMA = 1u << 1,
    CPU_PT_FEAT_FHM = 1u << 2,
    CPU_PT_FEAT_FP16 = 1u << 3,
    CPU_PT_FEAT_JSCVT = 1u << 4,
    CPU_PT_FEAT_LRCPC = 1u << 5,
    CPU_PT_FEAT_LSE = 1u << 6,
    CPU_PT_FEAT_RDM = 1u << 7,
};

static const cpu_pt_leaf_t cpu_pt_leaves[] = {
    {0x00000000u, 0, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 1, 0x8u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 2, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 4, 0x8u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 5, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 6, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 7, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 8, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 9, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 10, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 11, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 12, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 13, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 14, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 15, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 16, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 17, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 18, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 19, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 20, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 21, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 22, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 23, 0x8u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 24, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 25, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 26, 0x8u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 27, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 28, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 29, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 30, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 31, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 32, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 33, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 34, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 35, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 36, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 37, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 38, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 39, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 40, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 41, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 42, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 43, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 44, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 45, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 46, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 47, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 48, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 49, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 50, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 51, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 52, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 53, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 54, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 55, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 56, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 57, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 58, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 59, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 60, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 61, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 62, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 63, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 64, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 65, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 66, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 67, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 68, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 69, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 70, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 71, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 72, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 73, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 74, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 75, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 76, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 77, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 78, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 79, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 80, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 81, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 82, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 83, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 84, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 85, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 86, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 87, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 88, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 89, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 90, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 91, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 92, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 93, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 94, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 95, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 96, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 97, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 98, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 99, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 100, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 101, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 102, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 103, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 104, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 105, 0x10u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 106, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 107, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 108, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 109, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 110, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 111, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 112, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 113, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 114, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 115, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 116, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 117, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 118, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 119, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 120, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 121, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 122, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 123, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 124, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 125, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 126, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 127, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 128, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 129, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 130, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 131, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 132, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 133, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 134, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 135, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 136, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 137, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 138, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 139, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 140, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 141, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 142, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 143, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 144, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 145, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 146, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 147, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 148, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 149, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 150, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 151, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 152, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 153, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 154, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 155, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 156, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 157, 0x20u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 158, 0x20u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 159, 0x20u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 160, 0x20u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 161, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 162, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 163, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 164, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 165, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 166, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 167, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 168, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 169, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 170, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 171, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 172, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 173, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 174, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 175, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 176, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 178, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 180, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 182, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 184, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 186, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 188, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 190, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 192, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 194, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 196, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 198, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 200, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 202, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 204, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 206, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 208, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 210, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 212, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 214, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 216, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 218, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 220, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 222, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 224, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 226, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 228, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 230, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 232, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 234, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 236, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 238, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 240, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 242, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 244, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 246, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 248, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 250, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 252, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 254, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 256, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 258, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 260, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 262, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 264, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 266, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 268, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 270, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 272, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 274, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 276, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 278, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 280, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 282, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 284, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 286, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 288, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 290, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 292, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 294, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 296, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00100000u, 298, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 300, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 304, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 308, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 312, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 316, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 320, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 324, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 328, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 332, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 336, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 340, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 344, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 348, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 352, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 356, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 360, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 364, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 368, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 372, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 376, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 380, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 384, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 388, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 392, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 396, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 400, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 404, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 408, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 412, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 416, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 420, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 424, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 428, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 432, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 436, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 440, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 444, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 448, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 452, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 456, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 460, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 468, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 476, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 484, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 492, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 500, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 508, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 516, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 524, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 532, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001fe000u, 540, 0x0u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001fe000u, 796, 0x8u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001fe000u, 1052, 0x0u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x000703e0u, 1308, 0x0u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x000703e0u, 1564, 0x0u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x000703e0u, 1820, 0x0u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2076, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2084, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2092, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2100, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2108, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2116, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2124, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2132, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2140, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2148, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2156, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2164, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2172, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2180, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2188, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2196, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2204, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2212, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2220, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2228, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2236, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2244, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 2252, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x400703e0u, 2260, 0x8u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x400703e0u, 2772, 0x0u, {{0, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3284, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3285, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3286, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3287, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3288, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3289, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3290, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3291, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3292, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3293, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3294, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3295, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3296, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3297, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3298, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3299, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3303, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3307, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3311, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3315, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3319, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3323, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00010001u, 3327, 0x40u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3331, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3332, 0x40u, {{16, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {0, 5, CPU_PT_XR}, {16, 5, CPU_PT_XR}}},
    {0x00000000u, 3333, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3334, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3335, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3336, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3337, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3338, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3339, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3371, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3372, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3373, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3374, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3375, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3376, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3377, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3378, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3379, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3380, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3381, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3382, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3383, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3384, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3385, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3386, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3387, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3388, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3389, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3390, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3391, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3392, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3393, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3394, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3395, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3396, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3397, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3398, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3399, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3400, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3401, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3402, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3403, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3404, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3405, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3406, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3407, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3439, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3440, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3441, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3442, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3443, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3444, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3445, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3446, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3447, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3448, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3449, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3450, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3451, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3452, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3453, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3454, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3455, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3456, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3457, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3458, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3459, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3460, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3461, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3462, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3463, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3464, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3465, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3466, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3467, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3468, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3469, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3470, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3471, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3472, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3473, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3474, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3475, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3476, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3477, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3478, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3479, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3480, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3481, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3482, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3483, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3484, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3485, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3486, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3487, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3488, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3489, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3490, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3491, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3492, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3493, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3494, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3495, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3496, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3497, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3498, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3499, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3500, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3501, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3502, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3503, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3504, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3505, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3506, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3507, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3508, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3509, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3510, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3511, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3512, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3513, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3514, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3515, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3516, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3517, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3518, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3519, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3520, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3521, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3522, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3523, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3524, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3525, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3526, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3527, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3528, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3529, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3530, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3531, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3532, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3533, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3534, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3535, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3536, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3537, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3538, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3539, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3540, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3541, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3542, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3543, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3544, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3545, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3546, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3547, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3548, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3549, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3550, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3551, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3552, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3553, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3554, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3555, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3556, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3557, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3558, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3559, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3560, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3561, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3562, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3563, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3564, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3565, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3566, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3567, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3599, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3631, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3632, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3633, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3634, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3635, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3636, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3637, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3638, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3639, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3640, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3641, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3642, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3643, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3644, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3645, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3646, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3647, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3648, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3649, 0x40u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_XB}, {16, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}}},
    {0x001f0000u, 3650, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3682, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 3683, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3684, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3686, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3688, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3690, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3692, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x401f0000u, 3694, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x401f0000u, 3758, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3822, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3824, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3826, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3828, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3830, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3832, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3834, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3836, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3838, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3840, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3842, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3844, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3846, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 3848, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 3850, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 3852, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 3916, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 3980, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4044, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4108, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4172, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4236, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4300, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4364, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4428, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4492, 0x8u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4556, 0x0u, {{0, 5, CPU_PT_XW}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4620, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4622, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4624, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4626, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4628, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4630, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4632, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4634, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4636, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4638, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4640, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4642, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4644, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4646, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4648, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4650, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 4652, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4654, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4656, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 4658, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4660, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00400000u, 4662, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4664, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4666, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40000000u, 4668, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4670, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4734, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4798, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4862, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4926, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 4990, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5054, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5118, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5182, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5246, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5310, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5374, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5438, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5502, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5566, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5630, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5694, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5758, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x0000fc00u, 5822, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_XR}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5886, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 5950, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 6014, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x003f0000u, 6078, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6142, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6270, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6398, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6526, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6654, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6782, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 6910, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7038, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7166, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7294, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7422, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7550, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x007f0000u, 7678, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7806, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7810, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7814, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7818, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7822, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7826, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7830, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7834, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7838, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7842, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7846, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7850, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7854, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7858, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7862, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7866, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7870, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7874, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7878, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7886, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7890, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7898, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7902, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7910, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00300800u, 7914, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7922, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7926, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40400000u, 7930, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7934, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7938, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7942, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7946, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7950, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7954, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7958, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7962, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7966, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7970, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7974, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7978, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7982, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7986, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00c00000u, 7990, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 7994, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 8250, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 8506, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 8762, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9018, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9274, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9530, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 9786, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10042, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10298, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10554, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 10810, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11066, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11322, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11578, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 11834, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12090, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12346, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12602, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 12858, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13114, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13370, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13626, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 13882, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14138, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14394, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14650, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x407f0000u, 14906, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15162, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15170, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15178, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15186, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15194, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15202, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15210, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15218, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 15226, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 15234, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 15490, 0x8u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 15746, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 16002, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 16258, 0x8u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f00fu, 16514, 0x0u, {{0, 0, CPU_PT_NONE}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16770, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16786, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16802, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 16818, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16834, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16850, 0x4u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16866, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 16882, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16898, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 16914, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40300800u, 16930, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00700800u, 16946, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 16962, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 16970, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 16978, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 16986, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 16994, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17002, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17010, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17018, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17026, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17034, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17042, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17050, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17058, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17066, 0x1u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17074, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17082, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17090, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17098, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17106, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17114, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17122, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17130, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17138, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17146, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17154, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17162, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17170, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17178, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17186, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17194, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17202, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17210, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17218, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17226, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17234, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17242, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17250, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17258, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17266, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17274, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17282, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17290, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17298, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17306, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17314, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17322, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17330, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17338, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17346, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17354, 0x1u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17362, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17370, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17378, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17386, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17394, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17402, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17410, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17418, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17426, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17434, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17442, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17450, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17458, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17466, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17474, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17482, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17490, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17498, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17506, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17514, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17522, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c00000u, 17530, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c01000u, 17538, 0x2u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f000u, 17554, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f000u, 17570, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x0000f000u, 17586, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17602, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17634, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17666, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40700800u, 17698, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x001f7800u, 17730, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18242, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18274, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18306, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18338, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18370, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18402, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00f00800u, 18434, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40007800u, 18466, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40c01800u, 18498, 0x2u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x00000000u, 18530, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18531, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18532, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18533, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18534, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18535, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18536, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18537, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18538, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18539, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18540, 0x8u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x00000000u, 18541, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 5, CPU_PT_V}, {10, 5, CPU_PT_V}}},
    {0x40f00800u, 18542, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18606, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18670, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18734, 0x1u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18798, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18862, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18926, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 18990, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19054, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19118, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19182, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19246, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19310, 0x80u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19374, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19438, 0x1u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19502, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19566, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f00800u, 19630, 0x0u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
    {0x40f06800u, 19694, 0x2u, {{0, 5, CPU_PT_V}, {5, 5, CPU_PT_V}, {16, 4, CPU_PT_V}, {0, 0, CPU_PT_NONE}}},
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
                                            if ((word & 0x8c808000u) == 0x0c008000u && (((word >> 19) & 15u) != 0x0u)) return 623;
                                        } else {
                                            if ((word & 0x8c808000u) == 0x0c000000u && (((word >> 19) & 15u) != 0x0u)) return 633;
                                        }
                                    } else {
                                        if ((word & 0x8c008000u) == 0x0c008000u) return 819;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8c3f0c00u) == 0x0c300800u) return 215;
                                        } else {
                                            if ((word & 0x8c3f0c00u) == 0x0c300800u) return 211;
                                        }
                                    } else {
                                        if ((word & 0x8c3f0c00u) == 0x0c200800u) return 269;
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 505;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 503;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 513;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 511;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 507;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 501;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 515;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 509;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 504;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 502;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 512;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 510;
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 506;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 500;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 514;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 508;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x02000000u) {
                            if (word & 0x00000400u) {
                                if (word & 0x00000800u) {
                                    if ((word & 0xcd208000u) == 0x4c200000u) return 675;
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0xcda00000u) == 0x4ca00000u) return 549;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd600000u) == 0x4c600000u) return 538;
                                        } else {
                                            if ((word & 0xcd400000u) == 0x4d400000u) return 606;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0xcc008000u) == 0x4c008000u) return 818;
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0xcc768800u) == 0x4c708800u) return 153;
                                            } else {
                                                if ((word & 0xcc768800u) == 0x4c708800u) return 175;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0xcc368800u) == 0x4c208800u) return 229;
                                            } else {
                                                if ((word & 0xcc368800u) == 0x4c208800u) return 236;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00080000u) {
                                            if ((word & 0xccf68800u) == 0x4cf08800u) return 20;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0xcca68800u) == 0x4c208800u) return 177;
                                            } else {
                                                if ((word & 0xcca68800u) == 0x4ca08800u) return 185;
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
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 473;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 471;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 472;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 470;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 481;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 479;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 480;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 478;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 475;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 469;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 474;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 468;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 483;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 477;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 482;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 476;
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
                                        if ((word & 0x8f208000u) == 0x0e208000u) return 553;
                                    } else {
                                        if ((word & 0x8f208000u) == 0x0e208000u) return 561;
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 489;
                                        } else {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 487;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 491;
                                        } else {
                                            if ((word & 0x0f208800u) == 0x08200000u) return 485;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 488;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 486;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 490;
                                    } else {
                                        if ((word & 0x0f208c00u) == 0x08200000u) return 484;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x02000000u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00000400u) {
                                        if ((word & 0xcd208000u) == 0x4c208000u) return 551;
                                    } else {
                                        if ((word & 0xcd7f8000u) == 0x4c610000u) return 84;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x01000000u) {
                                            if ((word & 0xcc000400u) == 0x4c000400u && (((word >> 19) & 15u) != 0x0u)) return 631;
                                        } else {
                                            if ((word & 0xcc200400u) == 0x4c200400u) return 559;
                                        }
                                    } else {
                                        if ((word & 0xcd000400u) == 0x4d000400u && (((word >> 19) & 15u) != 0x0u)) return 627;
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 497;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 495;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 496;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 494;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 499;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 493;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 498;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 492;
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
                                        if ((word & 0xcf770400u) == 0x4e700000u) return 18;
                                    } else {
                                        if (word & 0x00100000u) {
                                            if ((word & 0xcf270400u) == 0x4e200000u) return 213;
                                        } else {
                                            if ((word & 0xcf270400u) == 0x4e200000u) return 181;
                                        }
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0xcf760400u) == 0x4e700000u) return 41;
                                        } else {
                                            if ((word & 0xcf360400u) == 0x4e200000u) return 191;
                                        }
                                    } else {
                                        if ((word & 0xcf3e0400u) == 0x4e300000u) return 209;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 160;
                                    } else {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 158;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 159;
                                    } else {
                                        if ((word & 0x0fff0400u) == 0x08bf0000u) return 157;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x02000000u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 676;
                                        } else {
                                            if ((word & 0x8d3f0000u) == 0x0c210000u) return 274;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8d600400u) == 0x0c600400u) return 539;
                                        } else {
                                            if ((word & 0x8d400400u) == 0x0d400400u) return 607;
                                        }
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 457;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 455;
                                            }
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 465;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 463;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 459;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 453;
                                            }
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 467;
                                            } else {
                                                if ((word & 0x0d200c00u) == 0x08200000u) return 461;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 456;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 454;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 464;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 462;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 458;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 452;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 466;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 460;
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
                                    if ((word & 0xcde08000u) == 0x4ce00000u) return 348;
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0xcd200000u) == 0x4c200000u) return 670;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd600000u) == 0x4c600000u) return 347;
                                        } else {
                                            if ((word & 0xcd400000u) == 0x4d400000u) return 619;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00010000u) {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd768800u) == 0x4c708800u) return 98;
                                        } else {
                                            if ((word & 0xcd768800u) == 0x4c708800u) return 55;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd368800u) == 0x4c208800u) return 208;
                                        } else {
                                            if ((word & 0xcd368800u) == 0x4c208800u) return 195;
                                        }
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0xcdfe0800u) == 0x4ce00800u) return 161;
                                    } else {
                                        if ((word & 0xcd3e0800u) == 0x4c200800u) return 277;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 441;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 439;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 440;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 438;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 449;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 447;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 448;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 446;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 443;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 437;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 442;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 436;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 451;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 445;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 450;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 444;
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
                                        if ((word & 0xcd008000u) == 0x4c000000u) return 677;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd408000u) == 0x4c400000u) return 353;
                                        } else {
                                            if ((word & 0xcd408000u) == 0x4c400000u) return 352;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0xcd608000u) == 0x4c400000u) return 359;
                                    } else {
                                        if (word & 0x01000000u) {
                                            if ((word & 0xcc408000u) == 0x4c400000u) return 618;
                                        } else {
                                            if ((word & 0xcc608000u) == 0x4c400000u) return 358;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0xcd778800u) == 0x4c718800u) return 83;
                                    } else {
                                        if ((word & 0xcd778800u) == 0x4c718800u) return 69;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xcd370800u) == 0x4c210800u) return 203;
                                        } else {
                                            if ((word & 0xcd370800u) == 0x4c210800u) return 199;
                                        }
                                    } else {
                                        if ((word & 0xcd370800u) == 0x4c210800u) return 271;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 425;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 423;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 424;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 422;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 433;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 431;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 432;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 430;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 427;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 421;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 426;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 420;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 435;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 429;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 434;
                                        } else {
                                            if ((word & 0x0d208c00u) == 0x08200000u) return 428;
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
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 632;
                                        } else {
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 626;
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 634;
                                        } else {
                                            if ((word & 0xcc800000u) == 0x4c000000u && (((word >> 19) & 15u) != 0x0u)) return 630;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0xcc001000u) == 0x4c001000u) return 730;
                                    } else {
                                        if ((word & 0xcc401000u) == 0x4c001000u) return 657;
                                    }
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0xccc01000u) == 0x4cc00000u) return 344;
                                        } else {
                                            if ((word & 0xcc001000u) == 0x4c000000u) return 669;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0xccc01000u) == 0x4cc00000u) return 520;
                                        } else {
                                            if ((word & 0xcc001000u) == 0x4c000000u) return 668;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if ((word & 0xccff0800u) == 0x4ce00800u) return 11;
                                    } else {
                                        if ((word & 0xccff0800u) == 0x4ce00800u) return 9;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 526;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 524;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 525;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 523;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 534;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 532;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 533;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 531;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 528;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 522;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 527;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 521;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 536;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 530;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 535;
                                        } else {
                                            if ((word & 0x0d201c00u) == 0x08200000u) return 529;
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
                                            if ((word & 0x8f200800u) == 0x0e000000u) return 351;
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 409;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 407;
                                            }
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 408;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 406;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 411;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 405;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 410;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 404;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x8f000800u) == 0x0f000000u) return 621;
                                        } else {
                                            if (word & 0x80000000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 417;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 415;
                                            }
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 416;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 414;
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 419;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 413;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 418;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 412;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 393;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 401;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 395;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 403;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x0f200000u) == 0x0e200000u) return 674;
                                        } else {
                                            if ((word & 0x0fc00000u) == 0x0f400000u) return 620;
                                        }
                                    } else {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 391;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 399;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 389;
                                            } else {
                                                if ((word & 0x0f200800u) == 0x08200000u) return 397;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 392;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 390;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 400;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 398;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 394;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 388;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 402;
                                        } else {
                                            if ((word & 0x0f200c00u) == 0x08200000u) return 396;
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
                        if (word & 0x01000000u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00000800u) {
                                    if ((word & 0x8e804000u) == 0x0e004000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 679;
                                } else {
                                    if ((word & 0xcef84000u) == 0x4e004000u) return 291;
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0xcef80800u) == 0x4e000000u) return 292;
                                    if ((word & 0xcef80800u) == 0x0e000000u) return 293;
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 697;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 703;
                                }
                            }
                        } else {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if ((word & 0x8ea04000u) == 0x0e204000u) return 643;
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8e200000u) == 0x0e200000u) return 637;
                                        } else {
                                            if ((word & 0x8e200000u) == 0x0e200000u) return 636;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 787;
                                    }
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if (word & 0x00200000u) {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 650;
                                            } else {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 646;
                                            }
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 642;
                                            } else {
                                                if ((word & 0x8e000000u) == 0x0e000000u) return 641;
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 805;
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 763;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 785;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00004000u) {
                            if (word & 0x00200000u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00001000u) {
                                        if ((word & 0x8f800000u) == 0x0e000000u) return 658;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f400000u) == 0x0e000000u) return 586;
                                        } else {
                                            if ((word & 0x8f400000u) == 0x0e000000u) return 583;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 635;
                                        } else {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 638;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 648;
                                        } else {
                                            if ((word & 0x8f000000u) == 0x0e000000u) return 644;
                                        }
                                    }
                                }
                            } else {
                                if ((word & 0x8f000000u) == 0x0e000000u) return 822;
                            }
                        } else {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 698;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 737;
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 686;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 709;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 762;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 700;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 734;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 782;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 690;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 773;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 761;
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
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 699;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 702;
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 684;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 705;
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 687;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 701;
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 691;
                                } else {
                                    if ((word & 0x8e800800u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 704;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 777;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 713;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 793;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 542;
                                                } else {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 544;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 543;
                                                } else {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 547;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 788;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 795;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 794;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 792;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 779;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 712;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 797;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 796;
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 786;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 784;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 798;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 783;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00800000u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if ((word & 0x8e404800u) == 0x0e400000u) return 581;
                                    } else {
                                        if ((word & 0x8e404800u) == 0x0e400000u) return 548;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8e406000u) == 0x0e402000u) return 552;
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e404000u) == 0x0e400000u) return 560;
                                        } else {
                                            if ((word & 0x8e404000u) == 0x0e400000u) return 579;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00400000u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 574;
                                            } else {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 591;
                                            }
                                        } else {
                                            if ((word & 0x8e006000u) == 0x0e002000u) return 550;
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 577;
                                            } else {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 554;
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 558;
                                            } else {
                                                if ((word & 0x8e004000u) == 0x0e000000u) return 575;
                                            }
                                        }
                                    }
                                } else {
                                    if ((word & 0xce000000u) == 0x4e000000u) return 813;
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
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 847;
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ef60800u) == 0x0ef00800u) return 230;
                                        } else {
                                            if ((word & 0x8eb60800u) == 0x0ea00800u) return 266;
                                        }
                                    } else {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ef60800u) == 0x0ef00800u) return 217;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xce660800u) == 0x4e200800u) return 113;
                                                } else {
                                                    if ((word & 0xce660800u) == 0x4e200800u) return 109;
                                                }
                                            } else {
                                                if ((word & 0x8ea60800u) == 0x0ea00800u) return 256;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 846;
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e760800u) == 0x0e700800u) return 228;
                                            } else {
                                                if ((word & 0x8e760800u) == 0x0e700800u) return 235;
                                            }
                                        } else {
                                            if ((word & 0x8ef60800u) == 0x0ef00800u) return 184;
                                        }
                                    } else {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 265;
                                            } else {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 273;
                                            }
                                        } else {
                                            if ((word & 0x8eb60800u) == 0x0ea00800u) return 242;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x01000000u) {
                                if (word & 0x00002000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 849;
                                } else {
                                    if ((word & 0x8ec00000u) == 0x0e800000u) return 725;
                                }
                            } else {
                                if (word & 0x00000800u) {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ef62000u) == 0x0e700000u) return 190;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e362000u) == 0x0e200000u) return 276;
                                            } else {
                                                if ((word & 0x8e362000u) == 0x0e200000u) return 245;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ef62000u) == 0x0ef00000u) return 180;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xce662000u) == 0x4e200000u) return 111;
                                                } else {
                                                    if ((word & 0xce662000u) == 0x4e200000u) return 107;
                                                }
                                            } else {
                                                if ((word & 0x8ea62000u) == 0x0ea00000u) return 240;
                                            }
                                        }
                                    }
                                } else {
                                    if ((word & 0x8e202000u) == 0x0e200000u) return 791;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00001000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x00080000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f770800u) == 0x0e710800u) return 207;
                                    } else {
                                        if ((word & 0x8f770800u) == 0x0e710800u) return 194;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f360800u) == 0x0e200800u) return 255;
                                        } else {
                                            if ((word & 0x8f360800u) == 0x0e200800u) return 248;
                                        }
                                    } else {
                                        if ((word & 0x8f360800u) == 0x0e200800u) return 294;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 811;
                                    } else {
                                        if ((word & 0x8e400000u) == 0x0e000000u) return 729;
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8e770800u) == 0x0e710800u) return 222;
                                        } else {
                                            if ((word & 0x8e770800u) == 0x0e710800u) return 226;
                                        }
                                    } else {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 259;
                                            } else {
                                                if ((word & 0x8e360800u) == 0x0e200800u) return 263;
                                            }
                                        } else {
                                            if ((word & 0x8e360800u) == 0x0e200800u) return 285;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 852;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 202;
                                            } else {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 198;
                                            }
                                        } else {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00010000u) {
                                                    if ((word & 0x8e260000u) == 0x0e200000u) return 313;
                                                } else {
                                                    if ((word & 0x8e260000u) == 0x0e200000u) return 312;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e270000u) == 0x0e210000u) return 253;
                                                } else {
                                                    if ((word & 0x8e270000u) == 0x0e210000u) return 250;
                                                }
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 790;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8ec00000u) == 0x0e800000u) return 721;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8ef60000u) == 0x0e700000u) return 221;
                                            } else {
                                                if ((word & 0x8eb60000u) == 0x0e200000u) return 258;
                                            }
                                        } else {
                                            if ((word & 0x8e3e0000u) == 0x0e200000u) return 283;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 789;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x01000000u) {
                        if (word & 0x00001000u) {
                            if ((word & 0x8e000000u) == 0x0e000000u) return 853;
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 851;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 850;
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 836;
                                } else {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 835;
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00004000u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e1f0000u) == 0x0e000000u) return 305;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x8e9f0000u) == 0x0e000000u) return 232;
                                            } else {
                                                if ((word & 0x8e9f0000u) == 0x0e000000u) return 231;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 778;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 776;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00010000u) {
                                            if ((word & 0x8e1e2000u) == 0x0e002000u) return 301;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x8e0e2000u) == 0x0e002000u) return 311;
                                            } else {
                                                if ((word & 0x8e0e2000u) == 0x0e002000u) return 315;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 800;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 781;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8ede0000u) == 0x0e400000u) return 204;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 309;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 739;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 307;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 310;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 799;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 314;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 281;
                                            }
                                        } else {
                                            if ((word & 0x8e1f0000u) == 0x0e000000u) return 296;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 738;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 780;
                                        }
                                    }
                                }
                            }
                        } else {
                            if ((word & 0x8ec00000u) == 0x0e000000u) return 821;
                        }
                    }
                }
            }
        }
    } else {
        if (word & 0x10000000u) {
            if (word & 0x40000000u) {
                if (word & 0x00004000u) {
                    if (word & 0x00001000u) {
                        if (word & 0x00002000u) {
                            if (word & 0x00000400u) {
                                if (word & 0x01000000u) {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8e808000u) == 0x0e008000u && (((word >> 19) & 15u) != 0x0u)) return 622;
                                    } else {
                                        if ((word & 0x8e808000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u)) return 628;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e208800u) == 0x0e208800u) return 595;
                                    } else {
                                        if ((word & 0x8e208800u) == 0x0e208800u) return 593;
                                    }
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ff60800u) == 0x0ef00800u) return 131;
                                        } else {
                                            if ((word & 0x8fb60800u) == 0x0ea00800u) return 220;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f7e0800u) == 0x0e300800u) return 112;
                                        } else {
                                            if ((word & 0x8f7e0800u) == 0x0e300800u) return 108;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 815;
                                    } else {
                                        if ((word & 0x8e3f0800u) == 0x0e200800u) return 268;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00008000u) {
                                if (word & 0x00000400u) {
                                    if ((word & 0x8fa00800u) == 0x0e200800u) return 590;
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 820;
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e760000u) == 0x0e700000u) return 130;
                                                    } else {
                                                        if ((word & 0x8e760000u) == 0x0e700000u) return 168;
                                                    }
                                                } else {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e360000u) == 0x0e200000u) return 219;
                                                    } else {
                                                        if ((word & 0x8e360000u) == 0x0e200000u) return 234;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00080000u) {
                                                    if ((word & 0x8ef60000u) == 0x0ef00000u) return 17;
                                                } else {
                                                    if (word & 0x00100000u) {
                                                        if ((word & 0x8ee60000u) == 0x0e200000u) return 16;
                                                    } else {
                                                        if ((word & 0x8ea60000u) == 0x0ea00000u) return 179;
                                                    }
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e200000u) == 0x0e200000u) return 667;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8f200000u) == 0x0e200000u) return 671;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f600000u) == 0x0e600000u) return 518;
                                        } else {
                                            if ((word & 0x8f400000u) == 0x0f400000u) return 605;
                                        }
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f000000u) == 0x0f000000u) return 728;
                                    } else {
                                        if ((word & 0x8f400000u) == 0x0f000000u) return 655;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00000400u) {
                            if (word & 0x00002000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e808800u) == 0x0e008000u && (((word >> 19) & 15u) != 0x0u)) return 624;
                                } else {
                                    if ((word & 0x8ea08800u) == 0x0e208000u) return 557;
                                }
                            } else {
                                if (word & 0x00000800u) {
                                    if ((word & 0x8f208000u) == 0x0e200000u) return 672;
                                } else {
                                    if ((word & 0x8fe08000u) == 0x0ee00000u) return 519;
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00080000u) {
                                    if ((word & 0x8ff78800u) == 0x0ef08800u) return 21;
                                } else {
                                    if ((word & 0x8fb78800u) == 0x0ea08800u) return 187;
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 816;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8e770800u) == 0x0e700800u) return 19;
                                            } else {
                                                if (word & 0x00100000u) {
                                                    if ((word & 0x8e670800u) == 0x0e200800u) return 110;
                                                } else {
                                                    if ((word & 0x8e270800u) == 0x0e200800u) return 183;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00080000u) {
                                                    if ((word & 0x8e760800u) == 0x0e700800u) return 34;
                                                } else {
                                                    if ((word & 0x8e360800u) == 0x0e200800u) return 189;
                                                }
                                            } else {
                                                if ((word & 0x8e7e0800u) == 0x0e300800u) return 106;
                                            }
                                        }
                                    }
                                } else {
                                    if ((word & 0x8f3f0800u) == 0x0e210800u) return 270;
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00000400u) {
                        if (word & 0x00002000u) {
                            if (word & 0x00000800u) {
                                if (word & 0x00200000u) {
                                    if (word & 0x00001000u) {
                                        if ((word & 0x8fc08000u) == 0x0ec00000u) return 345;
                                    } else {
                                        if ((word & 0x8f008000u) == 0x0e000000u) return 673;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f409000u) == 0x0e401000u) return 383;
                                    } else {
                                        if ((word & 0x8f409000u) == 0x0e401000u) return 382;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8f200000u) == 0x0e200000u) return 666;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f600000u) == 0x0e600000u) return 346;
                                        } else {
                                            if ((word & 0x8f400000u) == 0x0f400000u) return 609;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8ec08000u) == 0x0e400000u) return 608;
                                    } else {
                                        if ((word & 0x8ee08000u) == 0x0e400000u) return 357;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00000800u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8f800000u) == 0x0f000000u && (((word >> 19) & 15u) != 0x0u)) return 625;
                                    } else {
                                        if ((word & 0x8fe00000u) == 0x0e400000u) return 375;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8fe00000u) == 0x0ee00000u) return 349;
                                    } else {
                                        if ((word & 0x8f200000u) == 0x0e200000u) return 663;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8f800000u) == 0x0f000000u && (((word >> 19) & 15u) != 0x0u)) return 629;
                                    } else {
                                        if ((word & 0x8fc00000u) == 0x0f400000u) return 611;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8fe00000u) == 0x0ee00000u) return 319;
                                    } else {
                                        if (word & 0x00400000u) {
                                            if ((word & 0x8f800000u) == 0x0f000000u) return 610;
                                        } else {
                                            if ((word & 0x8fa00000u) == 0x0e000000u) return 350;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00002000u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 817;
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e670000u) == 0x0e610000u) return 91;
                                                    } else {
                                                        if ((word & 0x8e670000u) == 0x0e610000u) return 48;
                                                    }
                                                } else {
                                                    if ((word & 0x8ee70000u) == 0x0ee10000u) return 7;
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e2e0000u) == 0x0e200000u) return 206;
                                                    } else {
                                                        if ((word & 0x8e2e0000u) == 0x0e200000u) return 193;
                                                    }
                                                } else {
                                                    if ((word & 0x8eee0000u) == 0x0ee00000u) return 6;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x8e200000u) == 0x0e200000u) return 665;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 814;
                                    } else {
                                        if ((word & 0x8e3f0800u) == 0x0e200800u) return 272;
                                    }
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8f778800u) == 0x0e718800u) return 76;
                                    } else {
                                        if ((word & 0x8f778800u) == 0x0e718800u) return 62;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f368800u) == 0x0e208800u) return 201;
                                        } else {
                                            if ((word & 0x8f368800u) == 0x0e208800u) return 197;
                                        }
                                    } else {
                                        if ((word & 0x8ff68800u) == 0x0ee08800u) return 12;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x01000000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e001000u) == 0x0e001000u) return 732;
                                    } else {
                                        if ((word & 0x8e401000u) == 0x0e001000u) return 659;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e001000u) == 0x0e001000u) return 724;
                                    } else {
                                        if ((word & 0x8e401000u) == 0x0e001000u) return 653;
                                    }
                                }
                            } else {
                                if (word & 0x00000800u) {
                                    if (word & 0x00001000u) {
                                        if ((word & 0x8eff8000u) == 0x0ee08000u) return 8;
                                    } else {
                                        if ((word & 0x8eff8000u) == 0x0ee08000u) return 10;
                                    }
                                } else {
                                    if ((word & 0x8e209000u) == 0x0e209000u) return 664;
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x00800000u) {
                    if (word & 0x80000000u) {
                        if (word & 0x00010000u) {
                            if (word & 0x00020000u) {
                                if (word & 0x00040000u) {
                                    if (word & 0x00080000u) {
                                        if ((word & 0x0f70fc00u) == 0x0e200000u) return 126;
                                    } else {
                                        if ((word & 0x0f70fc00u) == 0x0e600000u) return 122;
                                    }
                                } else {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x0f58fc00u) == 0x0e400000u) return 172;
                                    } else {
                                        if ((word & 0x0f580000u) == 0x0e400000u) return 615;
                                    }
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00100000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f44fc00u) == 0x0e400000u) return 96;
                                        } else {
                                            if ((word & 0x0f440000u) == 0x0e400000u) return 572;
                                        }
                                    } else {
                                        if ((word & 0x0f64fc00u) == 0x0e600000u) return 81;
                                    }
                                } else {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f70fc00u) == 0x0e600000u) return 39;
                                    } else {
                                        if (word & 0x00100000u) {
                                            if ((word & 0x0f60fc00u) == 0x0e600000u) return 53;
                                        } else {
                                            if ((word & 0x0f60fc00u) == 0x0e600000u) return 67;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00020000u) {
                                if (word & 0x00040000u) {
                                    if (word & 0x00080000u) {
                                        if ((word & 0x0f70fc00u) == 0x0e200000u) return 118;
                                    } else {
                                        if ((word & 0x0f70fc00u) == 0x0e600000u) return 117;
                                    }
                                } else {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x0f58fc00u) == 0x0e400000u) return 165;
                                    } else {
                                        if ((word & 0x0f580000u) == 0x0e400000u) return 602;
                                    }
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00100000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f44fc00u) == 0x0e400000u) return 89;
                                        } else {
                                            if ((word & 0x0f440000u) == 0x0e400000u) return 566;
                                        }
                                    } else {
                                        if ((word & 0x0f64fc00u) == 0x0e600000u) return 74;
                                    }
                                } else {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f70fc00u) == 0x0e600000u) return 32;
                                    } else {
                                        if (word & 0x00100000u) {
                                            if ((word & 0x0f60fc00u) == 0x0e600000u) return 46;
                                        } else {
                                            if ((word & 0x0f60fc00u) == 0x0e600000u) return 60;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x01000000u) {
                                if (word & 0x00008000u) {
                                    if ((word & 0x0e400000u) == 0x0e400000u) return 833;
                                } else {
                                    if ((word & 0x0e400000u) == 0x0e400000u) return 830;
                                }
                            } else {
                                if (word & 0x00000800u) {
                                    if (word & 0x00000400u) {
                                        if ((word & 0x0e400000u) == 0x0e400000u) return 807;
                                    } else {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e408000u) == 0x0e400000u) return 370;
                                                } else {
                                                    if ((word & 0x0e408000u) == 0x0e400000u) return 385;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e408000u) == 0x0e400000u) return 373;
                                                } else {
                                                    if ((word & 0x0e408000u) == 0x0e400000u) return 361;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e408000u) == 0x0e400000u) return 364;
                                                } else {
                                                    if ((word & 0x0e408000u) == 0x0e400000u) return 355;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x0e408000u) == 0x0e400000u) return 367;
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x0e400000u) == 0x0e400000u) return 380;
                                                    } else {
                                                        if ((word & 0x0e400000u) == 0x0e400000u) return 377;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000010u) {
                                            if ((word & 0x0e400000u) == 0x0e400000u) return 716;
                                        } else {
                                            if ((word & 0x0e400000u) == 0x0e400000u) return 719;
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if ((word & 0x0e4003e0u) == 0x0e400000u) return 289;
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00020000u) {
                                                            if ((word & 0x0e5c2000u) == 0x0e440000u) return 136;
                                                        } else {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 151;
                                                            } else {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 155;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00020000u) {
                                                            if ((word & 0x0e5c2000u) == 0x0e400000u) return 99;
                                                        } else {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 145;
                                                            } else {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 14;
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00020000u) {
                                                            if ((word & 0x0e5c2000u) == 0x0e440000u) return 148;
                                                        } else {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 139;
                                                            } else {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 128;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00020000u) {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 133;
                                                            } else {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 104;
                                                            }
                                                        } else {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 142;
                                                            } else {
                                                                if ((word & 0x0e582000u) == 0x0e400000u) return 123;
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00000008u) {
                                                        if (word & 0x00000010u) {
                                                            if ((word & 0x0e5f8007u) == 0x0e400000u) return 1;
                                                        } else {
                                                            if ((word & 0x0e5f8007u) == 0x0e400000u) return 4;
                                                        }
                                                    } else {
                                                        if (word & 0x00000010u) {
                                                            if ((word & 0x0e408007u) == 0x0e400000u) return 23;
                                                        } else {
                                                            if ((word & 0x0e408007u) == 0x0e400000u) return 26;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00020000u) {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e588000u) == 0x0e400000u) return 121;
                                                            } else {
                                                                if ((word & 0x0e588000u) == 0x0e400000u) return 171;
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00100000u) {
                                                                    if ((word & 0x0e448000u) == 0x0e400000u) return 93;
                                                                } else {
                                                                    if ((word & 0x0e448000u) == 0x0e400000u) return 78;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e508000u) == 0x0e400000u) return 36;
                                                                } else {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e408000u) == 0x0e400000u) return 50;
                                                                    } else {
                                                                        if ((word & 0x0e408000u) == 0x0e400000u) return 64;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00020000u) {
                                                            if (word & 0x00040000u) {
                                                                if ((word & 0x0e588000u) == 0x0e400000u) return 114;
                                                            } else {
                                                                if ((word & 0x0e588000u) == 0x0e400000u) return 164;
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00100000u) {
                                                                    if ((word & 0x0e448000u) == 0x0e400000u) return 86;
                                                                } else {
                                                                    if ((word & 0x0e448000u) == 0x0e400000u) return 71;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e508000u) == 0x0e400000u) return 29;
                                                                } else {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e408000u) == 0x0e400000u) return 43;
                                                                    } else {
                                                                        if ((word & 0x0e408000u) == 0x0e400000u) return 57;
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
                                    if ((word & 0x0e400000u) == 0x0e400000u) return 827;
                                } else {
                                    if ((word & 0x0e400000u) == 0x0e400000u) return 824;
                                }
                            } else {
                                if (word & 0x00010000u) {
                                    if (word & 0x00020000u) {
                                        if ((word & 0x0e5c0000u) == 0x0e400000u) return 614;
                                    } else {
                                        if ((word & 0x0e5c0000u) == 0x0e580000u) return 569;
                                    }
                                } else {
                                    if (word & 0x00020000u) {
                                        if ((word & 0x0e5c0000u) == 0x0e400000u) return 601;
                                    } else {
                                        if ((word & 0x0e5c0000u) == 0x0e580000u) return 563;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00400000u) {
                        if (word & 0x80000000u) {
                            if (word & 0x00010000u) {
                                if (word & 0x00020000u) {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f38fc00u) == 0x0e200000u) return 119;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f18fc00u) == 0x0e000000u) return 170;
                                        } else {
                                            if ((word & 0x0f180000u) == 0x0e000000u) return 613;
                                        }
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x0f04fc00u) == 0x0e000000u) return 95;
                                            } else {
                                                if ((word & 0x0f040000u) == 0x0e000000u) return 571;
                                            }
                                        } else {
                                            if ((word & 0x0f24fc00u) == 0x0e200000u) return 80;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 38;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 52;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 66;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00020000u) {
                                    if (word & 0x00040000u) {
                                        if ((word & 0x0f38fc00u) == 0x0e200000u) return 116;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f18fc00u) == 0x0e000000u) return 163;
                                        } else {
                                            if ((word & 0x0f180000u) == 0x0e000000u) return 600;
                                        }
                                    }
                                } else {
                                    if (word & 0x00080000u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x0f04fc00u) == 0x0e000000u) return 88;
                                            } else {
                                                if ((word & 0x0f040000u) == 0x0e000000u) return 565;
                                            }
                                        } else {
                                            if ((word & 0x0f24fc00u) == 0x0e200000u) return 73;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 31;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 45;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 59;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00200000u) {
                                if (word & 0x01000000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 832;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 829;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x0e000000u) == 0x0e000000u) return 806;
                                        } else {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 369;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 384;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 372;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 360;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 363;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 354;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 366;
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 379;
                                                        } else {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 376;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000010u) {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 715;
                                            } else {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 718;
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x0e0003e0u) == 0x0e000000u) return 288;
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00040000u) {
                                                            if (word & 0x00010000u) {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 135;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 150;
                                                                }
                                                            } else {
                                                                if ((word & 0x0e1a2000u) == 0x0e000000u) return 144;
                                                            }
                                                        } else {
                                                            if (word & 0x00010000u) {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 101;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 154;
                                                                }
                                                            } else {
                                                                if ((word & 0x0e1a2000u) == 0x0e000000u) return 13;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e1c2000u) == 0x0e040000u) return 147;
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 138;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 127;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 132;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 103;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 141;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 120;
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
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 22;
                                                            } else {
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 25;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00100000u) {
                                                                    if ((word & 0x0e068000u) == 0x0e000000u) return 92;
                                                                } else {
                                                                    if ((word & 0x0e068000u) == 0x0e000000u) return 77;
                                                                }
                                                            } else {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e148000u) == 0x0e000000u) return 169;
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 35;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 49;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 63;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e148000u) == 0x0e140000u) return 105;
                                                                } else {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 85;
                                                                    } else {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 70;
                                                                    }
                                                                }
                                                            } else {
                                                                if (word & 0x00020000u) {
                                                                    if ((word & 0x0e148000u) == 0x0e000000u) return 162;
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 28;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 42;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 56;
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
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 826;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 823;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 612;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 568;
                                        }
                                    } else {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 599;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 562;
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
                                            if ((word & 0x0f06fc00u) == 0x0e000000u) return 97;
                                        } else {
                                            if ((word & 0x0f060000u) == 0x0e000000u) return 573;
                                        }
                                    } else {
                                        if ((word & 0x0f26fc00u) == 0x0e200000u) return 82;
                                    }
                                } else {
                                    if (word & 0x00020000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f14fc00u) == 0x0e000000u) return 174;
                                        } else {
                                            if ((word & 0x0f140000u) == 0x0e000000u) return 617;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 40;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 54;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 68;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00100000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f06fc00u) == 0x0e000000u) return 90;
                                        } else {
                                            if ((word & 0x0f060000u) == 0x0e000000u) return 567;
                                        }
                                    } else {
                                        if ((word & 0x0f26fc00u) == 0x0e200000u) return 75;
                                    }
                                } else {
                                    if (word & 0x00020000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x0f14fc00u) == 0x0e000000u) return 167;
                                        } else {
                                            if ((word & 0x0f140000u) == 0x0e000000u) return 604;
                                        }
                                    } else {
                                        if (word & 0x00040000u) {
                                            if ((word & 0x0f30fc00u) == 0x0e200000u) return 33;
                                        } else {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 47;
                                            } else {
                                                if ((word & 0x0f20fc00u) == 0x0e200000u) return 61;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00200000u) {
                                if (word & 0x01000000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 834;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 831;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00000400u) {
                                            if ((word & 0x0e000000u) == 0x0e000000u) return 808;
                                        } else {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 371;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 386;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 374;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 362;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 365;
                                                    } else {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 356;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x0e008000u) == 0x0e000000u) return 368;
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 381;
                                                        } else {
                                                            if ((word & 0x0e000000u) == 0x0e000000u) return 378;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000010u) {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 717;
                                            } else {
                                                if ((word & 0x0e000000u) == 0x0e000000u) return 720;
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x0e0003e0u) == 0x0e000000u) return 290;
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 137;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 102;
                                                                }
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 152;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 156;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e1c2000u) == 0x0e000000u) return 100;
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 146;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 15;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e1c2000u) == 0x0e040000u) return 149;
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 140;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 129;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if ((word & 0x0e1c2000u) == 0x0e040000u) return 134;
                                                            } else {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 143;
                                                                } else {
                                                                    if ((word & 0x0e182000u) == 0x0e000000u) return 125;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00002000u) {
                                                        if (word & 0x00000008u) {
                                                            if (word & 0x00000010u) {
                                                                if ((word & 0x0e1f8007u) == 0x0e000000u) return 2;
                                                            } else {
                                                                if ((word & 0x0e1f8007u) == 0x0e000000u) return 5;
                                                            }
                                                        } else {
                                                            if (word & 0x00000010u) {
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 24;
                                                            } else {
                                                                if ((word & 0x0e008007u) == 0x0e000000u) return 27;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 124;
                                                                } else {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 173;
                                                                }
                                                            } else {
                                                                if (word & 0x00080000u) {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 94;
                                                                    } else {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 79;
                                                                    }
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 37;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 51;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 65;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00020000u) {
                                                                if (word & 0x00040000u) {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 115;
                                                                } else {
                                                                    if ((word & 0x0e188000u) == 0x0e000000u) return 166;
                                                                }
                                                            } else {
                                                                if (word & 0x00080000u) {
                                                                    if (word & 0x00100000u) {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 87;
                                                                    } else {
                                                                        if ((word & 0x0e048000u) == 0x0e000000u) return 72;
                                                                    }
                                                                } else {
                                                                    if (word & 0x00040000u) {
                                                                        if ((word & 0x0e108000u) == 0x0e000000u) return 30;
                                                                    } else {
                                                                        if (word & 0x00100000u) {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 44;
                                                                        } else {
                                                                            if ((word & 0x0e008000u) == 0x0e000000u) return 58;
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
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 828;
                                    } else {
                                        if ((word & 0x0e000000u) == 0x0e000000u) return 825;
                                    }
                                } else {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 616;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 570;
                                        }
                                    } else {
                                        if (word & 0x00020000u) {
                                            if ((word & 0x0e1c0000u) == 0x0e000000u) return 603;
                                        } else {
                                            if ((word & 0x0e1c0000u) == 0x0e180000u) return 564;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            if (word & 0x00004000u) {
                if (word & 0x00000400u) {
                    if (word & 0x02000000u) {
                        if (word & 0x00000800u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00002000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8cf88000u) == 0x0c008000u) return 317;
                                        if ((word & 0x8c808000u) == 0x0c008000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 678;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8c200000u) == 0x0c200000u) return 661;
                                            } else {
                                                if ((word & 0x8c200000u) == 0x0c200000u) return 660;
                                            }
                                        } else {
                                            if ((word & 0x8c200000u) == 0x0c200000u) return 741;
                                        }
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8da00000u) == 0x0c200000u) return 656;
                                    } else {
                                        if ((word & 0x8d200000u) == 0x0c200000u) return 764;
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8d600000u) == 0x0c200000u) return 587;
                                        } else {
                                            if ((word & 0x8d600000u) == 0x0c200000u) return 584;
                                        }
                                    } else {
                                        if ((word & 0x8d200000u) == 0x0c200000u) return 752;
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 654;
                                        } else {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 652;
                                        }
                                    } else {
                                        if ((word & 0x8d200000u) == 0x0c200000u) return 765;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8d000000u) == 0x0c000000u) return 651;
                                            } else {
                                                if ((word & 0x8d000000u) == 0x0c000000u) return 647;
                                            }
                                        } else {
                                            if ((word & 0x8dd80000u) == 0x0d000000u) return 318;
                                        }
                                    } else {
                                        if (word & 0x01000000u) {
                                            if ((word & 0x8c800000u) == 0x0c000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 688;
                                        } else {
                                            if ((word & 0x8c200000u) == 0x0c200000u) return 743;
                                        }
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x01000000u) {
                                            if ((word & 0x8c800000u) == 0x0c000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 681;
                                        } else {
                                            if ((word & 0x8ca00000u) == 0x0c200000u) return 640;
                                        }
                                    } else {
                                        if ((word & 0x8d200000u) == 0x0c200000u) return 750;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 662;
                                        } else {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 639;
                                        }
                                    } else {
                                        if (word & 0x01000000u) {
                                            if ((word & 0x8c800000u) == 0x0c000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 682;
                                        } else {
                                            if ((word & 0x8c200000u) == 0x0c200000u) return 768;
                                        }
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 649;
                                        } else {
                                            if ((word & 0x8d200000u) == 0x0c200000u) return 645;
                                        }
                                    } else {
                                        if ((word & 0x8d200000u) == 0x0c200000u) return 769;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00008000u) {
                            if (word & 0x00400000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 325;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 323;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 335;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 324;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 322;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 334;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 333;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 331;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 339;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 332;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 330;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 338;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00400000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 327;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 321;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 337;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 326;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 320;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 336;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 343;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 329;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 341;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 342;
                                        } else {
                                            if ((word & 0x0d203800u) == 0x08203800u) return 328;
                                        }
                                    } else {
                                        if ((word & 0x8d203800u) == 0x08203800u) return 340;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00001000u) {
                        if (word & 0x00008000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x00080000u) {
                                    if ((word & 0x8ff70800u) == 0x0ef00800u) return 176;
                                } else {
                                    if (word & 0x00100000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f670800u) == 0x0e200800u) return 216;
                                        } else {
                                            if ((word & 0x8f670800u) == 0x0e200800u) return 212;
                                        }
                                    } else {
                                        if ((word & 0x8fa70800u) == 0x0ea00800u) return 238;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 848;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e760000u) == 0x0e700000u) return 218;
                                                } else {
                                                    if ((word & 0x8e760000u) == 0x0e700000u) return 233;
                                                }
                                            } else {
                                                if ((word & 0x8ef60000u) == 0x0ef00000u) return 178;
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 257;
                                                } else {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 267;
                                                }
                                            } else {
                                                if ((word & 0x8eb60000u) == 0x0ea00000u) return 239;
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 760;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 843;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e9e0000u) == 0x0e000000u) return 246;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 304;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 804;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 742;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 810;
                                    } else {
                                        if ((word & 0x8e400000u) == 0x0e000000u) return 727;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e1f0000u) == 0x0e000000u) return 287;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 802;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 740;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00002000u) {
                            if (word & 0x00008000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 838;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00080000u) {
                                            if ((word & 0x8ef70000u) == 0x0ef00000u) return 186;
                                        } else {
                                            if ((word & 0x8eb70000u) == 0x0ea00000u) return 243;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 736;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 840;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e9e0000u) == 0x0e000000u) return 251;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 298;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 775;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 772;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00008000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 844;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8ef60000u) == 0x0e700000u) return 188;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 275;
                                                } else {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 244;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00080000u) {
                                                if ((word & 0x8ef60000u) == 0x0ef00000u) return 182;
                                            } else {
                                                if (word & 0x00100000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x8e660000u) == 0x0e200000u) return 214;
                                                    } else {
                                                        if ((word & 0x8e660000u) == 0x0e200000u) return 210;
                                                    }
                                                } else {
                                                    if ((word & 0x8ea60000u) == 0x0ea00000u) return 241;
                                                }
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 755;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8ec00000u) == 0x0e800000u) return 726;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00010000u) {
                                            if ((word & 0x8e3e0000u) == 0x0e200000u) return 306;
                                        } else {
                                            if ((word & 0x8e3e0000u) == 0x0e200000u) return 280;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 706;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x00000400u) {
                    if (word & 0x00000800u) {
                        if (word & 0x00001000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x00200000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8f000000u) == 0x0e000000u) return 707;
                                    } else {
                                        if ((word & 0x8f000000u) == 0x0e000000u) return 710;
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x8f008000u) == 0x0e000000u) return 594;
                                        } else {
                                            if ((word & 0x8f008000u) == 0x0e000000u) return 592;
                                        }
                                    } else {
                                        if (word & 0x40000000u) {
                                            if ((word & 0x8f8f8000u) == 0x0e080000u) return 237;
                                        } else {
                                            if ((word & 0x8f808000u) == 0x0e000000u) return 537;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 685;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 735;
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8f000000u) == 0x0e000000u) return 597;
                                            } else {
                                                if ((word & 0x8f000000u) == 0x0e000000u) return 541;
                                            }
                                        } else {
                                            if ((word & 0x8f800000u) == 0x0e000000u) return 589;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8f000000u) == 0x0e000000u) return 598;
                                            } else {
                                                if ((word & 0x8f000000u) == 0x0e000000u) return 540;
                                            }
                                        } else {
                                            if ((word & 0xcf800000u) == 0x4e000000u) return 387;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00200000u) {
                                    if (word & 0x00008000u) {
                                        if ((word & 0x8f000000u) == 0x0e000000u) return 751;
                                    } else {
                                        if ((word & 0x8f000000u) == 0x0e000000u) return 766;
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if ((word & 0x8fc08000u) == 0x0e000000u) return 517;
                                    } else {
                                        if ((word & 0x8fc08000u) == 0x0e000000u) return 516;
                                    }
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 680;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 714;
                                    }
                                } else {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x8f000000u) == 0x0e000000u) return 756;
                                    } else {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8f000000u) == 0x0e000000u) return 588;
                                            } else {
                                                if ((word & 0x8f000000u) == 0x0e000000u) return 585;
                                            }
                                        } else {
                                            if ((word & 0x8f800000u) == 0x0e000000u) return 545;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00001000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x00008000u) {
                                    if ((word & 0x8f200000u) == 0x0e200000u) return 759;
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 693;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 711;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e400000u) == 0x0e400000u) return 582;
                                            } else {
                                                if ((word & 0x8e400000u) == 0x0e400000u) return 578;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 689;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 733;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 746;
                                        }
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 696;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 767;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e400000u) == 0x0e400000u) return 596;
                                            } else {
                                                if ((word & 0x8e400000u) == 0x0e400000u) return 555;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 694;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 749;
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 692;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 748;
                                        } else {
                                            if ((word & 0x8ec00000u) == 0x0e400000u) return 556;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 683;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 708;
                                    }
                                } else {
                                    if (word & 0x01000000u) {
                                        if ((word & 0x8e800000u) == 0x0e000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return 695;
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 747;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 580;
                                                } else {
                                                    if ((word & 0x8e000000u) == 0x0e000000u) return 576;
                                                }
                                            } else {
                                                if ((word & 0x8e800000u) == 0x0e000000u) return 546;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00001000u) {
                        if (word & 0x00002000u) {
                            if (word & 0x00008000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 845;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e670000u) == 0x0e610000u) return 205;
                                                } else {
                                                    if ((word & 0x8e670000u) == 0x0e610000u) return 192;
                                                }
                                            } else {
                                                if ((word & 0x8e270000u) == 0x0e210000u) return 279;
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e2e0000u) == 0x0e200000u) return 254;
                                                } else {
                                                    if ((word & 0x8e2e0000u) == 0x0e200000u) return 247;
                                                }
                                            } else {
                                                if ((word & 0x8e2e0000u) == 0x0e200000u) return 278;
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 758;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 842;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00100000u) {
                                                if ((word & 0x8e0f0000u) == 0x0e000000u) return 300;
                                            } else {
                                                if ((word & 0x8e0f0000u) == 0x0e000000u) return 308;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 803;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 771;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00008000u) {
                                if (word & 0x01000000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 812;
                                    } else {
                                        if ((word & 0x8e400000u) == 0x0e000000u) return 731;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 227;
                                            } else {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 223;
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 264;
                                                } else {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 260;
                                                }
                                            } else {
                                                if ((word & 0x8e360000u) == 0x0e200000u) return 282;
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 757;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x8e000000u) == 0x0e000000u) return 809;
                                    } else {
                                        if ((word & 0x8e400000u) == 0x0e000000u) return 723;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8e1f0000u) == 0x0e000000u) return 295;
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 801;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 745;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00002000u) {
                            if (word & 0x00008000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 841;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e670000u) == 0x0e610000u) return 200;
                                                } else {
                                                    if ((word & 0x8e670000u) == 0x0e610000u) return 196;
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if ((word & 0x8e260000u) == 0x0e200000u) return 303;
                                                } else {
                                                    if ((word & 0x8e260000u) == 0x0e200000u) return 302;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e2e0000u) == 0x0e200000u) return 252;
                                                } else {
                                                    if ((word & 0x8e2e0000u) == 0x0e200000u) return 249;
                                                }
                                            } else {
                                                if ((word & 0x8e2e0000u) == 0x0e200000u) return 286;
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 754;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 839;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00010000u) {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 316;
                                            } else {
                                                if ((word & 0x8e1e0000u) == 0x0e000000u) return 299;
                                            }
                                        } else {
                                            if ((word & 0x8e000000u) == 0x0e000000u) return 774;
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 770;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00008000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8e000000u) == 0x0e000000u) return 837;
                                } else {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 225;
                                            } else {
                                                if ((word & 0x8e770000u) == 0x0e710000u) return 224;
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 262;
                                                } else {
                                                    if ((word & 0x8e360000u) == 0x0e200000u) return 261;
                                                }
                                            } else {
                                                if ((word & 0x8e360000u) == 0x0e200000u) return 284;
                                            }
                                        }
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 753;
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x8ec00000u) == 0x0e800000u) return 722;
                                } else {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x8e3f0000u) == 0x0e200000u) return 297;
                                    } else {
                                        if ((word & 0x8e200000u) == 0x0e200000u) return 744;
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
