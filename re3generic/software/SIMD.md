# Optional AVX2 GLES pixel pipeline

The x64 Visual Studio build selects AVX2 at runtime after checking CPUID for
AVX, OSXSAVE, and AVX2, and XGETBV for OS-managed XMM/YMM state. Other CPUs use
the original scalar fragment path. `Renderer SIMD: AVX2` or `scalar` is printed
when the renderer is created. Set `RE3GENERIC_SIMD=scalar` before launching to
force the scalar path; remove the variable to restore automatic selection.

Only `SimdAvx2.cpp` is compiled with `/arch:AVX2`, with whole-program optimization
disabled for that file. The rest of GLES, the game, and the host retain their
baseline instruction set. No external library, thread, frame allocation, or
new port callback is introduced. Portable builds can compile `Simd.cpp` without
`RG_HAVE_AVX2` and omit `SimdAvx2.cpp`.

Complete eight-pixel triangle spans use vectorized interpolation, depth/scissor
tests, texture addressing/gathers, nearest or bilinear filtering, modulation or
replacement, fog, alpha tests, all existing blend factors, RGB565 packing, and
masked framebuffer/depth writes. Supported texture formats are RGBA8, RGB565,
RGBA4444, and RGBA5551. Untextured spans also use the vector path. Mipmap choice
and perspective span endpoints retain the existing calculations. Partial spans
of two to seven pixels also use the vector path when an eight-pixel buffer load
fits inside the row; masked stores preserve all neighboring pixels. Fully
depth-rejected rows skip interpolation, mipmap selection, and shading. Untextured
draws skip perspective/texture-coordinate work, as do inactive texture units.

Stencil, logic operations, additional texture units, other texture formats or
environment modes, one-pixel tails, and tails near the row boundary use the
original implementation. Point/line
rasterization and vertex transformation remain scalar. This is not a claim that
every GLES operation has been rewritten using AVX2. A single-vertex AVX2 fixed-
point matrix prototype was discarded because it was slower than scalar in the
local dependent-transform benchmark; forcing AVX2 everywhere is not beneficial.

Standalone validation compared 20,000 randomized complete spans byte-for-byte
against the original `Fragment`, including RGB565, alpha, depth, and surrounding
guards for span lengths one through eight. It covered both filters, four formats, 1x1 textures, wrapping, all depth/
alpha comparison and blend functions, fog, scissor, and write masks. Another
1,584 unaligned/masked buffer-clear tests checked boundaries. End-to-end GLES
framebuffer hashes matched across 144 scenes at six resolutions, including odd
dimensions and stencil fallback. These tests do not prove every game scene or
every possible GLES state is correct.

The early-depth kernel passed 160,000 variable-length reference checks, including
unaligned input, empty rows, all comparison functions, and clamping. Optimized
scalar and AVX2 hashes also matched the previous scalar renderer's 144 scenes.

## Optional real-game profiling

Set `RE3GENERIC_GLES_PROFILE=1` before starting a process to log two-second
averages for full frames, draw calls, triangles, scanlines, clears, and host
presentation. The nested draw/triangle/scanline timings must not be added
together. Pixel counters distinguish vectorized spans, scalar spans, scalar
tails, and entirely depth-culled rows. Profiling is disabled by default and
does not start a thread, but enabled timing has overhead and is not a substitute
for matched-scene performance comparisons.
Use `RE3GENERIC_GLES_PROFILE=30` to capture approximately the first thirty
seconds, then automatically disable all timing at a frame boundary. Values
greater than one specify a duration in seconds (up to 3600).

Performance must be compared at the same resolution and scene using scalar and
automatic selection. Synthetic renderer speedups are not equivalent to GTA III
FPS gains; vertex processing, triangle setup, unsupported states, and non-GLES
work are unchanged.
