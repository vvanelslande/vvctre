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
    set(BUILD_TOOLS OFF CACHE BOOL "")
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

# System Archives
add_library(system-archives INTERFACE)
target_include_directories(system-archives INTERFACE ./system-archives)

# curl
add_library(mbedcrypto::mbedcrypto INTERFACE IMPORTED)
set_target_properties(mbedcrypto::mbedcrypto PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/externals/mbedtls/include")
target_link_libraries(mbedcrypto::mbedcrypto INTERFACE mbedcrypto)
set(MBEDCRYPTO_LIBRARY mbedcrypto::mbedcrypto)

add_library(mbedtls::mbedtls INTERFACE IMPORTED)
set_target_properties(mbedtls::mbedtls PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/externals/mbedtls/include")
target_link_libraries(mbedtls::mbedtls INTERFACE mbedtls)
set(MBEDTLS_LIBRARY mbedtls::mbedtls)

add_library(mbedx509::mbedx509 INTERFACE IMPORTED)
set_target_properties(mbedx509::mbedx509 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/externals/mbedtls/include")
target_link_libraries(mbedx509::mbedx509 INTERFACE mbedtls)
set(MBEDX509_LIBRARY mbedx509::mbedx509)

set(MBEDTLS_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/externals/mbedtls/include")

set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(ENABLE_UNIX_SOCKETS OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_CRYPTO_AUTH ON CACHE BOOL "" FORCE)
set(CMAKE_USE_SECTRANSP OFF CACHE BOOL "" FORCE)
set(CMAKE_USE_SCHANNEL OFF CACHE BOOL "" FORCE)
set(CMAKE_USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(CMAKE_USE_BEARSSL OFF CACHE BOOL "" FORCE)
set(CMAKE_USE_NSS OFF CACHE BOOL "" FORCE)
set(CMAKE_USE_WOLFSSL OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory(curl EXCLUDE_FROM_ALL)

# llhttp
add_library(llhttp STATIC llhttp/src/api.c llhttp/src/http.c llhttp/src/llhttp.c llhttp/include/llhttp.h)
target_include_directories(llhttp INTERFACE llhttp/include)
target_include_directories(llhttp PRIVATE llhttp/include)

# whereami
add_library(whereami STATIC whereami/src/whereami.c whereami/src/whereami.h)
target_include_directories(whereami PRIVATE whereami/src)
target_include_directories(whereami INTERFACE whereami/src)

# json
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_ImplicitConversions OFF CACHE BOOL "" FORCE)
add_subdirectory(json EXCLUDE_FROM_ALL)

add_subdirectory(cryptopp)
add_subdirectory(teakra EXCLUDE_FROM_ALL)
add_subdirectory(portable-file-dialogs EXCLUDE_FROM_ALL)
