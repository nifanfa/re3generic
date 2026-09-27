#include "GlesProfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RG_GlesProfile *rg_gles_profile = 0;
static RG_GlesProfile counters = {};
static std::chrono::steady_clock::time_point interval_start;
static std::chrono::steady_clock::time_point capture_start;
static int capture_seconds;
static uint64_t frames;

void rg_profile_init()
{
#ifdef _MSC_VER
    char *setting = 0;
    size_t length = 0;
    _dupenv_s(&setting, &length, "RE3GENERIC_GLES_PROFILE");
    int seconds = setting ? atoi(setting) : 0;
    free(setting);
#else
    const char *value = getenv("RE3GENERIC_GLES_PROFILE");
    int seconds = value ? atoi(value) : 0;
#endif
    bool enabled = seconds > 0 && seconds <= 3600;
    capture_seconds = seconds > 1 ? seconds : 0;
    rg_gles_profile = enabled ? &counters : 0;
    counters = {};
    frames = 0;
    if (enabled) capture_start = interval_start = std::chrono::steady_clock::now();
}

void rg_profile_frame()
{
    if (!rg_gles_profile) return;
    ++frames;
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double, std::milli>(now - interval_start).count();
    if (elapsed < 2000) return;
    const double divisor = double(frames) * 1000000;
    const uint64_t pixels = counters.avx_pixels + counters.scalar_pixels + counters.tail_pixels;
    fprintf(stderr, "GLES profile: frame=%.2f draw=%.2f triangles=%.2f scanlines=%.2f clear=%.2f present=%.2f ms; draws=%llu vertices=%llu tris=%llu AVX=%.1f%% scalar=%llu tail=%llu\n",
        elapsed/frames,counters.draw_ns/divisor,counters.triangle_ns/divisor,counters.scanline_ns/divisor,
        counters.clear_ns/divisor,counters.present_ns/divisor,
        static_cast<unsigned long long>(counters.draws/frames),static_cast<unsigned long long>(counters.vertices/frames),
        static_cast<unsigned long long>(counters.triangles/frames),pixels ? 100.0*counters.avx_pixels/pixels : 0,
        static_cast<unsigned long long>(counters.scalar_pixels/frames),static_cast<unsigned long long>(counters.tail_pixels/frames));
    fprintf(stderr, "GLES formats:");
    for (int index = 0; index < 9; ++index) fprintf(stderr," %llu",static_cast<unsigned long long>(counters.formats[index]/frames));
    fprintf(stderr, "; modes:");
    for (int index = 0; index < 7; ++index) fprintf(stderr," %llu",static_cast<unsigned long long>(counters.modes[index]/frames));
    fprintf(stderr,"\n");
    fprintf(stderr,"GLES early-depth culled pixels/frame: %llu\n",static_cast<unsigned long long>(counters.culled_pixels/frames));
    counters = {};
    frames = 0;
    interval_start = now;
    if (capture_seconds && std::chrono::duration<double>(now - capture_start).count() >= capture_seconds) {
        rg_gles_profile = 0;
        fprintf(stderr,"GLES profiling finished; timing disabled\n");
    }
}
