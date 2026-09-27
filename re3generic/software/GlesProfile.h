#ifndef RE3_GENERIC_GLES_PROFILE_H
#define RE3_GENERIC_GLES_PROFILE_H

#include <stdint.h>
#include <chrono>

struct RG_GlesProfile {
    uint64_t draw_ns, triangle_ns, scanline_ns, clear_ns, present_ns;
    uint64_t draws, vertices, triangles, avx_pixels, scalar_pixels, tail_pixels, culled_pixels;
    uint64_t formats[9], modes[7];
};

extern RG_GlesProfile *rg_gles_profile;
void rg_profile_init();
void rg_profile_frame();

class RG_ProfileScope {
    uint64_t *counter;
    std::chrono::steady_clock::time_point start;
public:
    explicit RG_ProfileScope(uint64_t *target) : counter(target) {
        if (counter) start = std::chrono::steady_clock::now();
    }
    ~RG_ProfileScope() {
        if (counter) *counter += std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count();
    }
};

#endif
