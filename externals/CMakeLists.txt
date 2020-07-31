# Definitions for all external bundled libraries

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

# Dynarmic
if (ARCHITECTURE_x86_64)
    # Dynarmic will skip defining xbyak if it's already defined, we then define it below
    add_library(xbyak INTERFACE)
    option(DYNARMIC_TESTS OFF)
    set(DYNARMIC_NO_BUNDLED_FMT ON)
    set(DYNARMIC_FRONTENDS "A32")
    add_subdirectory(dynarmic)
endif()

# libfmt
add_subdirectory(fmt)
add_library(fmt::fmt ALIAS fmt)

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers INTERFACE ./nihstro/include)

# SoundTouch
add_subdirectory(soundtouch)
# The SoundTouch target doesn't export the necessary include paths as properties by default
target_include_directories(SoundTouch INTERFACE ./soundtouch/include)

# Xbyak
if (ARCHITECTURE_x86_64)
    # Defined before "dynarmic" above
    # add_library(xbyak INTERFACE)
    target_include_directories(xbyak INTERFACE ./xbyak/xbyak)
    target_compile_definitions(xbyak INTERFACE XBYAK_NO_OP_NAMES)
endif()

# ENet
add_subdirectory(enet)
target_include_directories(enet INTERFACE ./enet/include)

# Cubeb
if (ENABLE_CUBEB)
    set(BUILD_TESTS OFF CACHE BOOL "")
    add_subdirectory(cubeb EXCLUDE_FROM_ALL)
endif()

# glad
add_library(glad STATIC
    glad/src/glad.c
    glad/include/KHR/khrplatform.h
    glad/include/glad/glad.h)
target_include_directories(glad PUBLIC ./glad/include)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(glad PRIVATE dl)
endif()

# Dear ImGui
add_library(imgui
    imgui/imconfig.h
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui/examples/imgui_impl_opengl3.cpp
    imgui/examples/imgui_impl_opengl3.h
    imgui/examples/imgui_impl_sdl.cpp
    imgui/examples/imgui_impl_sdl.h
    imgui/misc/cpp/imgui_stdlib.cpp
    imgui/misc/cpp/imgui_stdlib.h)
target_link_libraries(imgui PRIVATE glad SDL2)
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_include_directories(imgui INTERFACE ./imgui ./imgui/examples ./imgui/misc/cpp)
target_include_directories(imgui PRIVATE ./imgui ./imgui/examples ./imgui/misc/cpp)

# clip
set(CLIP_EXAMPLES OFF CACHE BOOL "")
set(CLIP_TESTS OFF CACHE BOOL "")
if(UNIX)
    set(CLIP_X11_WITH_PNG ON)
endif()
add_subdirectory(clip EXCLUDE_FROM_ALL)
target_include_directories(clip INTERFACE ./clip)

# stb
add_library(stb
    stb/stb_image.h
    stb/stb_image_write.h
    stb/stb_image_resize.h
    stb_image.cpp
    stb_image_write.cpp
    stb_image_resize.cpp
)
target_include_directories(stb PUBLIC ./stb)

# mbedTLS
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(mbedtls EXCLUDE_FROM_ALL)

# ASL
add_library(asl STATIC
    asl/src/String.cpp
    asl/src/Socket.cpp
    asl/src/MulticastSocket.cpp
    asl/src/Http.cpp
    asl/src/Xdl.cpp
    asl/src/Var.cpp
    asl/src/IniFile.cpp
    asl/src/File.cpp
    asl/src/TextFile.cpp
    asl/src/Directory.cpp
    asl/src/Path.cpp
    asl/src/Date.cpp
    asl/src/Process.cpp
    asl/src/unicodedata.cpp
    asl/src/util.cpp
    asl/src/TlsSocket.cpp
    asl/include/asl/defs.h
    asl/include/asl/String.h
    asl/include/asl/Array.h
    asl/include/asl/Array_.h
    asl/include/asl/Stack.h
    asl/include/asl/Map.h
    asl/include/asl/HashMap.h
    asl/include/asl/Vec2.h
    asl/include/asl/Vec3.h
    asl/include/asl/Vec4.h
    asl/include/asl/File.h
    asl/include/asl/IniFile.h
    asl/include/asl/Date.h
    asl/include/asl/File.h
    asl/include/asl/TextFile.h
    asl/include/asl/Directory.h
    asl/include/asl/Path.h
    asl/include/asl/Mutex.h
    asl/include/asl/Process.h
    asl/include/asl/Var.h
    asl/include/asl/Xdl.h
    asl/include/asl/Socket.h
    asl/include/asl/Http.h
    asl/include/asl/Pointer.h
    asl/include/asl/util.h
    asl/include/asl/TlsSocket.h
)
target_compile_definitions(asl PUBLIC ASL_IPV6 ASL_TLS ASL_STATIC)
target_include_directories(asl PRIVATE ./asl/include ./mbedtls/include)
target_include_directories(asl INTERFACE ./asl/include)
target_link_libraries(asl PRIVATE mbedcrypto mbedx509 mbedtls)
if(WIN32)
    target_link_libraries(asl PRIVATE ws2_32 shell32 advapi32)
endif()

# System Archives
add_library(system-archives INTERFACE)
target_include_directories(system-archives INTERFACE ./system-archives)

add_subdirectory(cryptopp)
add_subdirectory(teakra EXCLUDE_FROM_ALL)
add_subdirectory(portable-file-dialogs EXCLUDE_FROM_ALL)
