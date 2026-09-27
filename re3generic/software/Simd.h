#ifndef RE3_GENERIC_SIMD_H
#define RE3_GENERIC_SIMD_H

#include <stddef.h>
#include <stdint.h>

namespace EGL { class RasterizerState; class Texture; }

struct RG_Span8 {
    const EGL::RasterizerState *state;
    const EGL::Texture *texture;
    uint16_t *color;
    uint8_t *alpha;
    uint32_t *depth;
    int32_t x;
    int32_t color_start[4];
    int32_t color_step[4];
    int32_t depth_start, depth_step;
    int32_t u_start, u_step, v_start, v_step;
    int32_t fog_start, fog_step;
    int32_t count = 8;
};

struct RG_SimdOps {
    void (*span8)(const RG_Span8 &span);
    bool (*depth_visible)(const uint32_t *buffer, int32_t count, int32_t start, int32_t step, int comparison);
    void (*clear32)(uint32_t *buffer, size_t count, uint32_t value, uint32_t mask);
    void (*clear16)(uint16_t *buffer, size_t count, uint16_t value, uint16_t mask);
    void (*fill8)(uint8_t *buffer, size_t count, uint8_t value);
    const char *name;
};

const RG_SimdOps &rg_simd_ops();

#ifdef RG_HAVE_AVX2
void rg_avx2_span8(const RG_Span8 &span);
bool rg_avx2_depth_visible(const uint32_t *buffer, int32_t count, int32_t start, int32_t step, int comparison);
void rg_avx2_clear32(uint32_t *buffer, size_t count, uint32_t value, uint32_t mask);
void rg_avx2_clear16(uint16_t *buffer, size_t count, uint16_t value, uint16_t mask);
void rg_avx2_fill8(uint8_t *buffer, size_t count, uint8_t value);
#endif

#endif
