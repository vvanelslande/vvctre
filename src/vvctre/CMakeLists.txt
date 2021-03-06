set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(vvctre
    vvctre.cpp
    common.cpp
    common.h
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    applets/swkbd.cpp
    applets/swkbd.h
    applets/mii_selector.cpp
    applets/mii_selector.h
    camera/image.cpp
    camera/image.h
    camera/tcp_client_rgb24_640x480.cpp
    camera/tcp_client_rgb24_640x480.h
    camera/util.cpp
    camera/util.h
    function_logger.cpp
    function_logger.h
    initial_settings.cpp
    initial_settings.h
    plugins.cpp
    plugins.h
)

create_target_directory_groups(vvctre)

target_link_libraries(vvctre PRIVATE common core input_common audio_core network)
target_link_libraries(vvctre PRIVATE stb glad clip portable_file_dialogs imgui cryptopp libcurl mbedtls whereami nlohmann_json flags llhttp ${PLATFORM_LIBRARIES} Threads::Threads)
target_include_directories(vvctre PRIVATE ${PROJECT_SOURCE_DIR}/externals/mbedtls/include)

if(WIN32)
    add_custom_command(TARGET vvctre POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2_DLL_DIR}/SDL2.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
else()
    target_link_libraries(vvctre PRIVATE dl)
endif()
