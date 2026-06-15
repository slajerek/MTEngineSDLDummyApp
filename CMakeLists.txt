cmake_minimum_required(VERSION 3.12)
project(MTEngineSDLDummyApp)

#set(CMAKE_VERBOSE_MAKEFILE on)

#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

add_definitions(
    -DLINUX
    -DIMGUI_IMPL_OPENGL_LOADER_GL3W
    -DENABLE_IMGUI_TEST_ENGINE
)

set(CMAKE_CXX_FLAGS "-fpermissive")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-macro-redefined")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-incompatible-pointer-types")

find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})
find_package(ALSA REQUIRED)

include_directories(
    # MTEngineSDL platform headers (Linux)
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/platform/Linux/src.Linux"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/platform/Linux/src.Linux/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Embedded"

    # Engine subsystems
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Audio/MIDI"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Audio/WAV"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Core"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Core/Net"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Core/Pool"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Core/Render"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Core/GamePads"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Funct"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Funct/ContinuousParam"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Funct/DataTable"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Funct/FFT"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Funct/ImageProcessing"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Funct/ImageProcessing/resampler"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Funct/ImageProcessing/stb_image"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/AudioMixer"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/Controls"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/Fonts"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/Helpers"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/Helpers/LoadingScreen"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/Helpers/ResourceManager"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/Layout"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/GUI/Waveform"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/ENet"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/hjson"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/imguiext"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/implot"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/imgui-notify"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/imgui_test_engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/imgui_test_engine/thirdparty"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/imgui_test_engine/thirdparty/Str"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/jpeg"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/jpeg/jpeg-9a"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/libjson"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/libjson/_internal/Dependencies/libbase64++"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/libjson/_internal/Dependencies/mempool++"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/libjson/_internal/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/libjson/_internal/Source/JSONDefs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/libpng"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/lodepng"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/md5"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/minizip"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/mtrand"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/nfd"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/nlohmann"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/pugixml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/rtmidi"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/sgSHA256"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/httplib"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/tremor"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/tremor/Ogg"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/utf8"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/uWebSockets"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Libs/zlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Tools"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/src/Engine/Tools/NetGame"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/other/lib/mbedtls/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/other/lib/mbedtls/library"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/other/lib/ftxui/include"

    # Application folders
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Views"
)

add_executable(MTEngineSDLDummyApp
    "${CMAKE_CURRENT_SOURCE_DIR}/src/DummyInit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Views/CMainMenuBar.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Views/CViewDummyAppMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/CDummyAppTestSuite.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/CTestAppStartup.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Tests/CImGuiDummyTests.cpp"
)

target_link_libraries(MTEngineSDLDummyApp
    GLEW
    GL
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/build/libMTEngineSDL.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/platform/Linux/libs/libmbedtls_bundle.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/platform/Linux/libs/uSockets.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MTEngineSDL/platform/Linux/libs/libftxui.a"
    xcb
    glib-2.0
    gobject-2.0
    gio-2.0
    dbus-1
    sndio
    dl
    m
    pthread
    freetype
    X11
    z
    ${GTK3_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${ALSA_LIBRARIES}
)
