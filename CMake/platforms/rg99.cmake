# RG99 has the same layout as RG300 but only 32 MiB RAM
set(BUILD_ASSETS_MPQ OFF)
set(UNPACKED_MPQS ON)
set(UNPACKED_SAVES ON)
set(NONET ON)
set(USE_SDL1 ON)

# Link `libstdc++` dynamically: ~1.3 MiB.
# The OPK is mounted as squashfs and the binary is decompressed, while
# the system `libstdc++` resides on disk.
set(DEVILUTIONX_STATIC_CXX_STDLIB OFF)

# -fmerge-all-constants saves ~4 KiB
# -fsection-anchors saves ~4 KiB
set(_extra_flags "-fmerge-all-constants -fsection-anchors")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_extra_flags}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_extra_flags}")
# -Wl,-z-stack-size: the default thread stack size for RG99 is 128 KiB, reduce it.
#   https://wiki.musl-libc.org/functional-differences-from-glibc.html#Thread-stack-size
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,stack-size=32768")

# 128 KiB
set(DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT OFF)

# Must stream most of the audio due to RAM constraints.
set(STREAM_ALL_AUDIO_MIN_FILE_SIZE 4096)

# Must use a smaller audio buffer due to RAM constraints.
set(DEFAULT_AUDIO_BUFFER_SIZE 768)

# Use lower resampling quality for FPS.
set(DEFAULT_AUDIO_RESAMPLING_QUALITY 2)

# RG-99 hardware scaler can only scale YUV.
# The SDL library on RG-99 can convert 8-bit palettized surfaces to YUV automatically.
set(SDL1_VIDEO_MODE_BPP 8)
set(SDL1_FORCE_SVID_VIDEO_MODE ON)
set(SDL1_FORCE_DIRECT_RENDER ON)

# Must be an HWSURFACE for the scaler to work.
set(SDL1_VIDEO_MODE_FLAGS SDL_HWSURFACE|SDL_FULLSCREEN)

# Videos are 320x240, so they fit in video ram double-buffered.
set(SDL1_VIDEO_MODE_SVID_FLAGS SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF)

set(PREFILL_PLAYER_NAME ON)
set(HAS_KBCTRL 1)
set(DEVILUTIONX_GAMEPAD_TYPE Nintendo)
set(KBCTRL_BUTTON_DPAD_LEFT SDLK_LEFT)
set(KBCTRL_BUTTON_DPAD_RIGHT SDLK_RIGHT)
set(KBCTRL_BUTTON_DPAD_UP SDLK_UP)
set(KBCTRL_BUTTON_DPAD_DOWN SDLK_DOWN)
set(KBCTRL_BUTTON_B SDLK_LCTRL)
set(KBCTRL_BUTTON_A SDLK_LALT)
set(KBCTRL_BUTTON_Y SDLK_SPACE)
set(KBCTRL_BUTTON_X SDLK_LSHIFT)
set(KBCTRL_BUTTON_RIGHTSHOULDER SDLK_BACKSPACE)
set(KBCTRL_BUTTON_LEFTSHOULDER SDLK_TAB)
set(KBCTRL_BUTTON_START SDLK_RETURN)
set(KBCTRL_BUTTON_LEFTSTICK SDLK_END) # Suspend
set(KBCTRL_BUTTON_BACK SDLK_ESCAPE) # Select
set(KBCTRL_IGNORE_1 SDLK_3) # Backlight
