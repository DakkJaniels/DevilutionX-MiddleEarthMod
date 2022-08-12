# General build options.
set(BUILD_TESTING OFF)

# Disable all system dependencies.
# All of these will be fetched via FetchContent and linked statically.
set(DEVILUTIONX_SYSTEM_SDL2 OFF)

# Static SDL2 on Android requires Position Independent Code.
set(SDL_STATIC_PIC ON)

set(DEVILUTIONX_SYSTEM_SDL_IMAGE OFF)
set(DEVILUTIONX_SYSTEM_SDL_AUDIOLIB OFF)
set(DEVILUTIONX_SYSTEM_LIBSODIUM OFF)
set(DEVILUTIONX_SYSTEM_LIBPNG OFF)
set(DEVILUTIONX_SYSTEM_LIBFMT OFF)
set(DEVILUTIONX_SYSTEM_BZIP2 OFF)

# Package the assets with the APK.
set(BUILD_ASSETS_MPQ OFF)
set(DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/android-project/app/src/main/assets")

# Disable sanitizers. They're not supported out-of-the-box.
set(ASAN OFF)
set(UBSAN OFF)

# Disable in-game options to exit the game.
set(NOEXIT ON)
