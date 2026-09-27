# re3generic

Experimental single-threaded framebuffer port of re3. This repository contains
the re3-symbian game source, librw, Vincent software GLES, the generic adapter,
and a Windows host. It does not contain GTA III game data.

## Build

Open `re3generic.slnx` in Visual Studio and build `re3host` for x64. No CMake
installation or CMake-generated files are required. The Debug x64 host builds
to `re3generic/x64/Debug/re3host.exe`.

Each project uses a separate intermediate directory under
`re3generic/obj/<platform>/<configuration>/<project>/`, preventing same-named
object files in the game, librw, and Vincent from overwriting each other.
Both solution builds and direct project builds place final outputs in
`re3generic/<platform>/<configuration>/`.

## Run

Pass the directory of an original GTA III installation to the executable:

```
re3generic\x64\Debug\re3host.exe "C:\path\to\GTA III"
```

The default framebuffer and window client area are both 320×240. To choose a
different internal resolution, append width and height (up to 1024 each), for
example `re3host.exe "C:\path\to\GTA III" 480 360`. Resizing the window keeps
the original aspect ratio and fills unused space with black.

Texture interpolation keeps determinant intermediates in 64 bits until all
fixed-point scaling is complete, preventing inverted images at higher
resolutions. A standalone orientation check passed at 320x240, 480x360,
640x480, 800x600, and 1024x1024; the 640x480 game menu was also visually verified.

The host checks for `models/gta3.img` and `data/gta3.dat`. The Windows host
opens a window, sends keyboard events to re3, and presents Vincent's RGB565
framebuffer with 24-bit depth and synchronous resource reads. The Vincent
framebuffer stores the bottom scanline first. The
Release x64 host has reached the main menu, started a new game, and rendered
the world with original GTA III assets. Distant geometry flicker, radar, and
HUD text have received visual validation. RGB modulation no longer wraps bright
colors to black. Reaching gameplay does not establish that every subsystem or
operating-system audio service runs on one thread.

## Port interface

`re3generic/include/re3generic.h` defines callbacks for presentation, input,
time, random-access file reads, and optional PCM output. Bind a single `RG_Port` before calling
`rg_game_init`, then call `rg_game_step` on the same thread and finish with
`rg_game_shutdown`. `re3generic/include/re3generic_game.h` declares those game
functions. The host must keep file handles open until `file_close`; reads may
stall the frame.

## Audio

The generic sample backend mixes original `AUDIO/SFX.SDT` and `AUDIO/SFX.RAW`
effects and two file streams into signed 16-bit, interleaved stereo PCM at
32000 Hz. WAV streams support 16-bit PCM and IMA ADPCM; MP3 uses the bundled
minimp3 decoder (upstream revision `ea99364f61c14656440e8d77e9c233ccf3124633`).
Compressed streams are decoded in bounded buffers, not loaded entirely into RAM.
Decoding, mixing, and `submit_pcm` calls run on the game thread; no application
audio worker thread is created.

The Windows host plays PCM with `waveOut` and eight fixed 512-frame buffers.
Windows and audio drivers can use their own threads. Device errors are logged;
the host can continue without playback. Other ports can omit `submit_pcm` or
provide their own nonblocking playback sink. Set `RE3GENERIC_PCM_FILE` to an
output path to also save raw PCM for diagnostics. Effects, streamed speech, and
PCM playback have been tested; subjective playback was confirmed by the user.
