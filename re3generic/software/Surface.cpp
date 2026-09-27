#include "stdafx.h"
#include "Surface.h"
#include "Simd.h"
#include "GlesProfile.h"

namespace EGL {

Surface::Surface(const Config &config)
    : m_Config(config),
      m_Rect(0, 0, config.GetConfigAttrib(EGL_WIDTH),
             config.GetConfigAttrib(EGL_HEIGHT)),
      m_ColorBuffer(new U16[m_Rect.width * m_Rect.height]()),
      m_AlphaBuffer(new U8[m_Rect.width * m_Rect.height]()),
      m_DepthBuffer(new U32[m_Rect.width * m_Rect.height]()),
      m_StencilBuffer(new U32[m_Rect.width * m_Rect.height]()),
      m_CurrentContext(0)
{
}

Surface::~Surface()
{
    delete[] m_ColorBuffer;
    delete[] m_AlphaBuffer;
    delete[] m_DepthBuffer;
    delete[] m_StencilBuffer;
}

void Surface::Dispose()
{
    if (m_CurrentContext == 0)
        delete this;
}

void Surface::ClearDepthBuffer(U32 depth, bool mask, const Rect &scissor)
{
    RG_ProfileScope scope(rg_gles_profile ? &rg_gles_profile->clear_ns : 0);
    if (!mask)
        return;
    Rect clipped = Rect::Intersect(m_Rect, scissor);
    if (clipped.width <= 0 || clipped.height <= 0)
        return;
    const RG_SimdOps &operations = rg_simd_ops();
    for (int row = clipped.y; row < clipped.y + clipped.height; ++row)
        operations.clear32(m_DepthBuffer + row * m_Rect.width + clipped.x,
                           clipped.width, depth, UINT32_MAX);
}

void Surface::ClearStencilBuffer(U32 value, U32 mask, const Rect &scissor)
{
    RG_ProfileScope scope(rg_gles_profile ? &rg_gles_profile->clear_ns : 0);
    Rect clipped = Rect::Intersect(m_Rect, scissor);
    if (clipped.width <= 0 || clipped.height <= 0)
        return;
    const RG_SimdOps &operations = rg_simd_ops();
    for (int row = clipped.y; row < clipped.y + clipped.height; ++row)
        operations.clear32(m_StencilBuffer + row * m_Rect.width + clipped.x,
                           clipped.width, value, mask);
}

void Surface::ClearColorBuffer(const Color &rgba, const Color &mask,
                               const Rect &scissor)
{
    RG_ProfileScope scope(rg_gles_profile ? &rg_gles_profile->clear_ns : 0);
    U16 color = rgba.ConvertTo565();
    U16 color_mask = mask.ConvertTo565();
    Rect clipped = Rect::Intersect(m_Rect, scissor);
    if (clipped.width <= 0 || clipped.height <= 0)
        return;
    const RG_SimdOps &operations = rg_simd_ops();
    for (int row = clipped.y; row < clipped.y + clipped.height; ++row) {
        int index = row * m_Rect.width + clipped.x;
        operations.clear16(m_ColorBuffer + index, clipped.width, color, color_mask);
        if (mask.A())
            operations.fill8(m_AlphaBuffer + index, clipped.width, rgba.A());
    }
}

}
