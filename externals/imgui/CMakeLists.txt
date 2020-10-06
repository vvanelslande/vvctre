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
target_compile_definitions(imgui PUBLIC -DIMGUI_USER_CONFIG=\"imgui_config.h\")
target_include_directories(imgui INTERFACE ./imgui ./imgui/examples ./imgui/misc/cpp ./config)
target_include_directories(imgui PRIVATE ./imgui ./imgui/examples ./imgui/misc/cpp ./config)
