#include "Simd.h"
#include <stdlib.h>
#include <string.h>
#ifdef RG_HAVE_AVX2
#include <intrin.h>
#endif

static void clear32_scalar(uint32_t *buffer, size_t count, uint32_t value, uint32_t mask)
{
    for (size_t index = 0; index < count; ++index)
        buffer[index] = (buffer[index] & ~mask) | (value & mask);
}

static void clear16_scalar(uint16_t *buffer, size_t count, uint16_t value, uint16_t mask)
{
    for (size_t index = 0; index < count; ++index)
        buffer[index] = (buffer[index] & ~mask) | (value & mask);
}

static void fill8_scalar(uint8_t *buffer, size_t count, uint8_t value)
{
    memset(buffer, value, count);
}

static RG_SimdOps select_simd()
{
    RG_SimdOps result = { 0, 0, clear32_scalar, clear16_scalar, fill8_scalar, "scalar" };
#ifdef RG_HAVE_AVX2
    char *setting = 0;
    size_t length = 0;
    _dupenv_s(&setting, &length, "RE3GENERIC_SIMD");
    const bool disabled = setting && strcmp(setting, "scalar") == 0;
    free(setting);
    if (disabled)
        return result;
    int registers[4] = {};
    __cpuid(registers, 0);
    if (registers[0] < 7)
        return result;
    __cpuidex(registers, 1, 0);
    const int required = (1 << 27) | (1 << 28);
    if ((registers[2] & required) != required || (_xgetbv(0) & 6) != 6)
        return result;
    __cpuidex(registers, 7, 0);
    if (!(registers[1] & (1 << 5)))
        return result;
    result.span8 = rg_avx2_span8;
    result.depth_visible = rg_avx2_depth_visible;
    result.clear32 = rg_avx2_clear32;
    result.clear16 = rg_avx2_clear16;
    result.fill8 = rg_avx2_fill8;
    result.name = "AVX2";
#endif
    return result;
}

const RG_SimdOps &rg_simd_ops()
{
    static const RG_SimdOps operations = select_simd();
    return operations;
}
