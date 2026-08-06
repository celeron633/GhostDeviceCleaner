cmake_minimum_required(VERSION 3.24)

project(RemoveGhostDevices VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PRIVATE UNICODE _UNICODE)

add_executable(RemoveGhostDevices WIN32
    src/main.cpp
    src/application.cpp
    src/application.h
    src/admin_utils.cpp
    src/admin_utils.h
    src/device_filter.cpp
    src/device_filter.h
    src/device_manager.cpp
    src/device_manager.h
    src/text_utils.cpp
    src/text_utils.h
)

target_compile_features(RemoveGhostDevices PRIVATE cxx_std_20)
target_compile_definitions(RemoveGhostDevices PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_options(RemoveGhostDevices PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
)
target_link_libraries(RemoveGhostDevices PRIVATE
    imgui
    d3d11
    dxgi
    setupapi
    shell32
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RemoveGhostDevices)

