cmake_minimum_required(VERSION 3.22)
project(ragemod CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

add_library(ragemod SHARED
    src/main.cpp
    src/il2cpp.cpp
    src/hook.cpp
    src/game.cpp
    src/mods.cpp
    src/config.cpp
    src/touch.cpp
    src/ui.cpp
    src/zygisk_entry.cpp
    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ragemod PRIVATE src ${IMGUI} ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zygisk)

target_compile_definitions(ragemod PRIVATE
    IMGUI_IMPL_OPENGL_ES3
    IMGUI_DISABLE_DEMO_WINDOWS
    IMGUI_DISABLE_DEBUG_TOOLS
)

target_compile_options(ragemod PRIVATE
    -O2 -fno-rtti -fno-exceptions -fvisibility=hidden
    -ffunction-sections -fdata-sections
    -Wall -Wno-unused-parameter -Wno-unused-function
)

target_link_options(ragemod PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384)

target_link_libraries(ragemod log android EGL GLESv3)
