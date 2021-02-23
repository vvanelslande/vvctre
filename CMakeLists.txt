# CMake 3.8 required for 17 to be a valid value for CXX_STANDARD
cmake_minimum_required(VERSION 3.8)
include(CMakeDependentOption)

project(vvctre)

option(ENABLE_CUBEB "Enable the Cubeb audio output sink and real device microphone backend" ON)
CMAKE_DEPENDENT_OPTION(ENABLE_MF "Use Media Foundation AAC decoder" ON "WIN32" OFF)
CMAKE_DEPENDENT_OPTION(ENABLE_FDK "Use FDK AAC decoder" OFF "NOT ENABLE_MF" OFF)

# Configure C++ standard
# ===========================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set up output paths for executable binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)

# Boost
set(BOOST_ROOT "${PROJECT_SOURCE_DIR}/externals/boost")
set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/externals/boost")
set(Boost_NO_SYSTEM_PATHS OFF)
find_package(Boost QUIET REQUIRED)

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# SDL2
find_package(SDL2)

if(NOT SDL2_FOUND)
    if(WIN32 AND (MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1930))
        set(SDL_PREFIX "${PROJECT_BINARY_DIR}/externals/SDL2-2.0.14")

        if(NOT EXISTS "${SDL_PREFIX}")
            message(STATUS "Downloading SDL...")
            file(DOWNLOAD
                https://www.libsdl.org/release/SDL2-devel-2.0.14-VC.zip
                "${PROJECT_BINARY_DIR}/externals/sdl.zip" SHOW_PROGRESS)
            execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${PROJECT_BINARY_DIR}/externals/sdl.zip"
                            WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/externals")
        endif()

        set(SDL2_FOUND YES)
        set(SDL2_INCLUDE_DIRS "${SDL_PREFIX}/include")
        set(SDL2_LIBRARIES "${SDL_PREFIX}/lib/x64/SDL2.lib")
        set(SDL2_DLL_DIR "${SDL_PREFIX}/lib/x64")
    else()
        message(FATAL_ERROR "Install SDL2")
    endif()
endif()

# Some installations don't set SDL2_LIBRARIES
if("${SDL2_LIBRARIES}" STREQUAL "")
    set(SDL2_LIBRARIES "SDL2::SDL2")
endif()

add_library(SDL2 INTERFACE)
target_include_directories(SDL2 INTERFACE "${SDL2_INCLUDE_DIRS}")
target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARIES}")

# FDK
if(ENABLE_FDK)
    find_library(FDK_AAC fdk-aac DOC "The path to fdk_aac library")
    if(FDK_AAC STREQUAL "FDK_AAC-NOTFOUND")
        message(FATAL_ERROR "fdk_aac library not found.")
    endif()
endif()

# Platform-specific requirements
# ======================================

if(WIN32)
    # WSAPoll didn't exist before WinNT 6.x (Vista)
    add_definitions(-D_WIN32_WINNT=0x0600 -DWINVER=0x0600)
    set(PLATFORM_LIBRARIES winmm ws2_32)
else()
    set(PLATFORM_LIBRARIES rt)
endif()

# Include source code
# ===================

# This function should be passed a list of all files in a target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(create_target_directory_groups target_name)
    # Place any files that aren't in the source list in a separate group so that they don't get in
    # the way.
    source_group("Other Files" REGULAR_EXPRESSION ".")

    get_target_property(target_sources "${target_name}" SOURCES)

    foreach(file_name IN LISTS target_sources)
        get_filename_component(dir_name "${file_name}" PATH)
        # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
        string(REPLACE "/" "\\" group_name "${dir_name}")
        source_group("${group_name}" FILES "${file_name}")
    endforeach()
endfunction()

# Prevent Boost from linking against libs when building
add_definitions(
    -DBOOST_ERROR_CODE_HEADER_ONLY
    -DBOOST_SYSTEM_NO_LIB
    -DBOOST_DATE_TIME_NO_LIB
    -DBOOST_REGEX_NO_LIB
)

add_subdirectory(externals)
add_subdirectory(src)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vvctre)
