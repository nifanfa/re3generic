#include "Simd.h"
#ifdef RG_HAVE_AVX2
#include "stdafx.h"
#include "RasterizerState.h"
#include "Texture.h"
#include <immintrin.h>

using namespace EGL;

struct RG_Avx2Core {
    struct VectorColor {
        __m256i red, green, blue, alpha;
    };

    static __m256i broadcast(int32_t value)
    {
        return _mm256_set1_epi32(value);
    }

    static __m256i interpolate(int32_t start, int32_t step)
    {
        return _mm256_add_epi32(broadcast(start),
            _mm256_mullo_epi32(_mm256_setr_epi32(0,1,2,3,4,5,6,7), broadcast(step)));
    }

    static __m256i compare(__m256i incoming, __m256i stored, int function)
    {
        const __m256i ones = broadcast(-1);
        switch (function) {
        case RasterizerState::CompFuncLess: return _mm256_cmpgt_epi32(stored, incoming);
        case RasterizerState::CompFuncEqual: return _mm256_cmpeq_epi32(incoming, stored);
        case RasterizerState::CompFuncLEqual: return _mm256_xor_si256(_mm256_cmpgt_epi32(incoming, stored), ones);
        case RasterizerState::CompFuncGreater: return _mm256_cmpgt_epi32(incoming, stored);
        case RasterizerState::CompFuncNotEqual: return _mm256_xor_si256(_mm256_cmpeq_epi32(incoming, stored), ones);
        case RasterizerState::CompFuncGEqual: return _mm256_xor_si256(_mm256_cmpgt_epi32(stored, incoming), ones);
        case RasterizerState::CompFuncAlways: return ones;
        default: return _mm256_setzero_si256();
        }
    }

    static __m256i fixed_color(int32_t start, int32_t step)
    {
        __m256i value = _mm256_min_epi32(broadcast(65536),
            _mm256_max_epi32(_mm256_setzero_si256(), interpolate(start, step)));
        return _mm256_srli_epi32(_mm256_mullo_epi32(value, broadcast(511)), 17);
    }

    static __m256i multiply_color(__m256i first, __m256i second)
    {
        __m256i product = _mm256_mullo_epi32(first, second);
        return _mm256_srli_epi32(_mm256_add_epi32(product, _mm256_srli_epi32(product, 7)), 8);
    }

    static __m256i multiply_rgb_texture(__m256i first, __m256i second)
    {
        __m256i product = _mm256_add_epi32(_mm256_mullo_epi32(first, second), broadcast(128));
        return _mm256_srli_epi32(_mm256_add_epi32(product, _mm256_srli_epi32(product, 8)), 8);
    }

    static __m256i wrap(__m256i value, int mode)
    {
        if (mode == RasterizerState::WrappingModeClampToEdge)
            return _mm256_min_epi32(broadcast(65535), _mm256_max_epi32(value, _mm256_setzero_si256()));
        return _mm256_and_si256(value, broadcast(65535));
    }

    static VectorColor sample(const RG_Span8 &span, __m256i coord_u, __m256i coord_v)
    {
        const RasterizerState::TextureState &state = span.state->m_Texture[0];
        const int width = span.texture->GetLogWidth();
        const int height = span.texture->GetLogHeight();
        __m256i tex_x = _mm256_srlv_epi32(wrap(coord_u, state.WrappingModeS), broadcast(16-width));
        __m256i tex_y = _mm256_srlv_epi32(wrap(coord_v, state.WrappingModeT), broadcast(16-height));
        __m256i offset = _mm256_add_epi32(tex_x, _mm256_sllv_epi32(tex_y, broadcast(width)));
        const void *data = span.texture->GetData();
        const __m256i bytes = broadcast(255);
        if (state.InternalFormat == RasterizerState::TextureFormatRGBA8) {
            __m256i packed = _mm256_i32gather_epi32(static_cast<const int *>(data), offset, 4);
            return { _mm256_and_si256(packed, bytes),
                _mm256_and_si256(_mm256_srli_epi32(packed, 8), bytes),
                _mm256_and_si256(_mm256_srli_epi32(packed, 16), bytes), _mm256_srli_epi32(packed, 24) };
        }
        __m256i packed;
        if (width + height == 0) {
            packed = broadcast(*static_cast<const uint16_t *>(data));
        } else {
            packed = _mm256_i32gather_epi32(static_cast<const int *>(data), _mm256_srli_epi32(offset, 1), 4);
            packed = _mm256_and_si256(_mm256_srlv_epi32(packed,
                _mm256_slli_epi32(_mm256_and_si256(offset, broadcast(1)), 4)), broadcast(65535));
        }
        VectorColor result;
        if (state.InternalFormat == RasterizerState::TextureFormatRGBA4444) {
            result.red = _mm256_and_si256(_mm256_srli_epi32(packed, 8), broadcast(240));
            result.green = _mm256_and_si256(_mm256_srli_epi32(packed, 4), broadcast(240));
            result.blue = _mm256_and_si256(packed, broadcast(240));
            result.alpha = _mm256_slli_epi32(_mm256_and_si256(packed, broadcast(15)), 4);
            result.red = _mm256_or_si256(result.red, _mm256_srli_epi32(result.red, 4));
            result.green = _mm256_or_si256(result.green, _mm256_srli_epi32(result.green, 4));
            result.blue = _mm256_or_si256(result.blue, _mm256_srli_epi32(result.blue, 4));
            result.alpha = _mm256_or_si256(result.alpha, _mm256_srli_epi32(result.alpha, 4));
        } else {
            const bool rgb565 = state.InternalFormat == RasterizerState::TextureFormatRGB565;
            result.red = _mm256_and_si256(_mm256_srli_epi32(packed, 8), broadcast(248));
            result.green = _mm256_and_si256(_mm256_srli_epi32(packed, 3), broadcast(rgb565 ? 252 : 248));
            result.blue = rgb565 ? _mm256_slli_epi32(_mm256_and_si256(packed, broadcast(31)), 3) :
                _mm256_slli_epi32(_mm256_and_si256(packed, broadcast(62)), 2);
            result.red = _mm256_or_si256(result.red, _mm256_srli_epi32(result.red, 5));
            result.green = _mm256_or_si256(result.green, _mm256_srli_epi32(result.green, rgb565 ? 6 : 5));
            result.blue = _mm256_or_si256(result.blue, _mm256_srli_epi32(result.blue, 5));
            result.alpha = rgb565 ? bytes : _mm256_mullo_epi32(_mm256_and_si256(packed, broadcast(1)), bytes);
        }
        return result;
    }

    static __m256i mix_channel(__m256i first, __m256i second, __m256i amount)
    {
        return _mm256_and_si256(_mm256_srli_epi32(_mm256_add_epi32(
            _mm256_mullo_epi32(first, amount),
            _mm256_mullo_epi32(second, _mm256_sub_epi32(broadcast(256), amount))), 8), broadcast(255));
    }

    static VectorColor mix_color(const VectorColor &first, const VectorColor &second, __m256i amount)
    {
        return { mix_channel(first.red, second.red, amount), mix_channel(first.green, second.green, amount),
            mix_channel(first.blue, second.blue, amount), mix_channel(first.alpha, second.alpha, amount) };
    }

    static VectorColor texture_color(const RG_Span8 &span)
    {
        __m256i coord_u = interpolate(span.u_start, span.u_step);
        __m256i coord_v = interpolate(span.v_start, span.v_step);
        if (span.state->m_Texture[0].MinFilterMode != RasterizerState::FilterModeLinear)
            return sample(span, coord_u, coord_v);
        const int width = span.texture->GetLogWidth();
        const int height = span.texture->GetLogHeight();
        __m256i low_u = _mm256_sub_epi32(coord_u, broadcast(0x8000 >> width));
        __m256i high_u = _mm256_add_epi32(coord_u, broadcast(0x7fff >> width));
        __m256i low_v = _mm256_sub_epi32(coord_v, broadcast(0x8000 >> height));
        __m256i high_v = _mm256_add_epi32(coord_v, broadcast(0x7fff >> height));
        __m256i fraction_u = _mm256_srli_epi32(_mm256_and_si256(
            _mm256_sllv_epi32(low_u, broadcast(width)), broadcast(65535)), 8);
        __m256i fraction_v = _mm256_srli_epi32(_mm256_and_si256(
            _mm256_sllv_epi32(low_v, broadcast(height)), broadcast(65535)), 8);
        VectorColor high_row = mix_color(sample(span, high_u, high_v), sample(span, low_u, high_v), fraction_u);
        VectorColor low_row = mix_color(sample(span, high_u, low_v), sample(span, low_u, low_v), fraction_u);
        return mix_color(high_row, low_row, fraction_v);
    }

    static __m128i pack16(__m256i value)
    {
        return _mm_packus_epi32(_mm256_castsi256_si128(value), _mm256_extracti128_si256(value, 1));
    }

    static VectorColor destination_color(__m256i packed, __m256i alpha)
    {
        VectorColor result;
        result.red = _mm256_and_si256(_mm256_srli_epi32(packed, 8), broadcast(248));
        result.green = _mm256_and_si256(_mm256_srli_epi32(packed, 3), broadcast(252));
        result.blue = _mm256_slli_epi32(_mm256_and_si256(packed, broadcast(31)), 3);
        result.red = _mm256_or_si256(result.red, _mm256_srli_epi32(result.red, 5));
        result.green = _mm256_or_si256(result.green, _mm256_srli_epi32(result.green, 6));
        result.blue = _mm256_or_si256(result.blue, _mm256_srli_epi32(result.blue, 5));
        result.alpha = alpha;
        return result;
    }

    static VectorColor blend_coefficient(int function, bool source, const VectorColor &incoming, const VectorColor &stored)
    {
        const __m256i zero = _mm256_setzero_si256();
        const __m256i maximum = broadcast(255);
        const VectorColor &other = source ? stored : incoming;
        __m256i amount;
        switch (function) {
        case 1: return { maximum, maximum, maximum, maximum };
        case 2: return other;
        case 3: return { _mm256_sub_epi32(maximum, other.red), _mm256_sub_epi32(maximum, other.green),
            _mm256_sub_epi32(maximum, other.blue), _mm256_sub_epi32(maximum, other.alpha) };
        case 4: amount = incoming.alpha; break;
        case 5: amount = _mm256_sub_epi32(maximum, incoming.alpha); break;
        case 6: amount = stored.alpha; break;
        case 7: amount = _mm256_sub_epi32(maximum, stored.alpha); break;
        case 8:
            amount = _mm256_min_epi32(incoming.alpha, _mm256_sub_epi32(maximum, stored.alpha));
            return { amount, amount, amount, maximum };
        default: amount = zero; break;
        }
        return { amount, amount, amount, amount };
    }

    static __m256i blend_channel(__m256i incoming, __m256i stored, __m256i src_coefficient, __m256i dst_coefficient)
    {
        return _mm256_min_epi32(broadcast(255), _mm256_add_epi32(
            multiply_color(incoming, src_coefficient), multiply_color(stored, dst_coefficient)));
    }

    static void span8(const RG_Span8 &span)
    {
        const RasterizerState &state = *span.state;
        __m256i passed = broadcast(-1);
        if (span.count != 8)
            passed = compare(_mm256_setr_epi32(0,1,2,3,4,5,6,7), broadcast(span.count), RasterizerState::CompFuncLess);
        if (state.m_ScissorTest.Enabled) {
            __m256i offset = _mm256_sub_epi32(interpolate(span.x, 1), broadcast(state.m_ScissorTest.X));
            passed = _mm256_and_si256(passed, _mm256_and_si256(compare(offset, _mm256_setzero_si256(), RasterizerState::CompFuncGEqual),
                compare(offset, broadcast(state.m_ScissorTest.Width), RasterizerState::CompFuncLess)));
        }
        __m256i depth = _mm256_min_epi32(broadcast(0xffffff),
            _mm256_max_epi32(interpolate(span.depth_start, span.depth_step), _mm256_setzero_si256()));
        if (state.m_DepthTest.Enabled)
            passed = _mm256_and_si256(passed, compare(depth,
                _mm256_loadu_si256(reinterpret_cast<const __m256i *>(span.depth)), state.m_DepthTest.Func));
        if (_mm256_testz_si256(passed, passed)) return;
        VectorColor color = { fixed_color(span.color_start[0], span.color_step[0]),
            fixed_color(span.color_start[1], span.color_step[1]), fixed_color(span.color_start[2], span.color_step[2]),
            fixed_color(span.color_start[3], 0) };
        if (span.texture) {
            VectorColor texel = texture_color(span);
            if (state.m_Texture[0].Mode == RasterizerState::TextureModeReplace) {
                if (state.m_Texture[0].InternalFormat == RasterizerState::TextureFormatRGB565)
                    texel.alpha = color.alpha;
                color = texel;
            } else {
                if (state.m_Texture[0].InternalFormat == RasterizerState::TextureFormatRGB565) {
                    color.red = multiply_rgb_texture(color.red, texel.red);
                    color.green = multiply_rgb_texture(color.green, texel.green);
                    color.blue = multiply_rgb_texture(color.blue, texel.blue);
                } else {
                    color.red = multiply_color(color.red, texel.red);
                    color.green = multiply_color(color.green, texel.green);
                    color.blue = multiply_color(color.blue, texel.blue);
                    color.alpha = multiply_color(color.alpha, texel.alpha);
                }
            }
        }
        if (state.m_Fog.Enabled) {
            __m256i amount = _mm256_srai_epi32(interpolate(span.fog_start, span.fog_step), 8);
            color.red = mix_channel(color.red, broadcast(state.m_Fog.Color.r), amount);
            color.green = mix_channel(color.green, broadcast(state.m_Fog.Color.g), amount);
            color.blue = mix_channel(color.blue, broadcast(state.m_Fog.Color.b), amount);
        }
        if (state.m_Alpha.Enabled) {
            const uint8_t reference = static_cast<uint8_t>((state.m_Alpha.Reference * 255) >> 16);
            passed = _mm256_and_si256(passed, compare(color.alpha, broadcast(reference), state.m_Alpha.Func));
            if (_mm256_testz_si256(passed, passed)) return;
        }
        const __m128i original_color = _mm_loadu_si128(reinterpret_cast<const __m128i *>(span.color));
        const __m128i original_alpha = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(span.alpha));
        if (state.m_Blend.Enabled) {
            VectorColor stored = destination_color(_mm256_cvtepu16_epi32(original_color),
                _mm256_cvtepu8_epi32(original_alpha));
            VectorColor src_coefficient = blend_coefficient(state.m_Blend.FuncSrc, true, color, stored);
            VectorColor dst_coefficient = blend_coefficient(state.m_Blend.FuncDst, false, color, stored);
            color = { blend_channel(color.red, stored.red, src_coefficient.red, dst_coefficient.red),
                blend_channel(color.green, stored.green, src_coefficient.green, dst_coefficient.green),
                blend_channel(color.blue, stored.blue, src_coefficient.blue, dst_coefficient.blue),
                blend_channel(color.alpha, stored.alpha, src_coefficient.alpha, dst_coefficient.alpha) };
        }
        __m256i packed = _mm256_setzero_si256();
        if (state.m_Mask.Red) packed = _mm256_slli_epi32(_mm256_and_si256(color.red, broadcast(248)), 8);
        if (state.m_Mask.Green) packed = _mm256_or_si256(packed,
            _mm256_slli_epi32(_mm256_and_si256(color.green, broadcast(252)), 3));
        if (state.m_Mask.Blue) packed = _mm256_or_si256(packed, _mm256_srli_epi32(color.blue, 3));
        const __m128i mask16 = _mm_packs_epi32(_mm256_castsi256_si128(passed), _mm256_extracti128_si256(passed, 1));
        _mm_storeu_si128(reinterpret_cast<__m128i *>(span.color), _mm_blendv_epi8(original_color, pack16(packed), mask16));
        if (state.m_Mask.Alpha) {
            const __m128i mask8 = _mm_packs_epi16(mask16, _mm_setzero_si128());
            const __m128i alpha8 = _mm_packus_epi16(pack16(color.alpha), _mm_setzero_si128());
            _mm_storel_epi64(reinterpret_cast<__m128i *>(span.alpha), _mm_blendv_epi8(original_alpha, alpha8, mask8));
        }
        if (state.m_Mask.Depth)
            _mm256_maskstore_epi32(reinterpret_cast<int *>(span.depth), passed, depth);
    }
};

void rg_avx2_span8(const RG_Span8 &span)
{
    RG_Avx2Core::span8(span);
}

bool rg_avx2_depth_visible(const uint32_t *buffer, int32_t count, int32_t start, int32_t step, int comparison)
{
    if (count <= 0) return false;
    if (comparison == RasterizerState::CompFuncAlways) return true;
    if (comparison == RasterizerState::CompFuncNever) return false;
    __m256i incoming = RG_Avx2Core::interpolate(start, step);
    const __m256i increment = _mm256_set1_epi32(static_cast<int32_t>(uint32_t(step) * 8));
    int32_t index = 0;
    for (; index + 8 <= count; index += 8) {
        __m256i clipped = _mm256_min_epi32(_mm256_set1_epi32(0xffffff),
            _mm256_max_epi32(incoming, _mm256_setzero_si256()));
        __m256i passed = RG_Avx2Core::compare(clipped,
            _mm256_loadu_si256(reinterpret_cast<const __m256i *>(buffer + index)), comparison);
        if (!_mm256_testz_si256(passed, passed)) return true;
        incoming = _mm256_add_epi32(incoming, increment);
    }
    for (; index < count; ++index) {
        int32_t depth = static_cast<int32_t>(uint32_t(start) + uint32_t(step) * index);
        depth = depth < 0 ? 0 : depth > 0xffffff ? 0xffffff : depth;
        int32_t stored = static_cast<int32_t>(buffer[index]);
        bool passed;
        switch (comparison) {
        case RasterizerState::CompFuncLess: passed = depth < stored; break;
        case RasterizerState::CompFuncEqual: passed = depth == stored; break;
        case RasterizerState::CompFuncLEqual: passed = depth <= stored; break;
        case RasterizerState::CompFuncGreater: passed = depth > stored; break;
        case RasterizerState::CompFuncNotEqual: passed = depth != stored; break;
        case RasterizerState::CompFuncGEqual: passed = depth >= stored; break;
        default: passed = false; break;
        }
        if (passed) return true;
    }
    return false;
}

void rg_avx2_clear32(uint32_t *buffer, size_t count, uint32_t value, uint32_t mask)
{
    const __m256i masked = _mm256_set1_epi32(static_cast<int32_t>(value & mask));
    const __m256i keep = _mm256_set1_epi32(static_cast<int32_t>(~mask));
    size_t index = 0;
    for (; index + 8 <= count; index += 8) {
        __m256i result = masked;
        if (mask != UINT32_MAX) result = _mm256_or_si256(masked,
            _mm256_and_si256(_mm256_loadu_si256(reinterpret_cast<const __m256i *>(buffer + index)), keep));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(buffer + index), result);
    }
    for (; index < count; ++index) buffer[index] = (buffer[index] & ~mask) | (value & mask);
}

void rg_avx2_clear16(uint16_t *buffer, size_t count, uint16_t value, uint16_t mask)
{
    const __m256i masked = _mm256_set1_epi16(value & mask);
    const __m256i keep = _mm256_set1_epi16(static_cast<int16_t>(~mask));
    size_t index = 0;
    for (; index + 16 <= count; index += 16) {
        __m256i result = masked;
        if (mask != UINT16_MAX) result = _mm256_or_si256(masked,
            _mm256_and_si256(_mm256_loadu_si256(reinterpret_cast<const __m256i *>(buffer + index)), keep));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(buffer + index), result);
    }
    for (; index < count; ++index) buffer[index] = (buffer[index] & ~mask) | (value & mask);
}

void rg_avx2_fill8(uint8_t *buffer, size_t count, uint8_t value)
{
    const __m256i packed = _mm256_set1_epi8(value);
    size_t index = 0;
    for (; index + 32 <= count; index += 32)
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(buffer + index), packed);
    for (; index < count; ++index) buffer[index] = value;
}
#endif
