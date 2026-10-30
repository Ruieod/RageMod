cmake_minimum_required(VERSION 3.22)
project(ragemod_cs2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# imgui общий с модулем standoff2, второй копии в репе не держим
set(IMGUI ${CMAKE_CURRENT_SOURCE_DIR}/../standoff2/third_party/imgui)
set(FONT_TTF ${CMAKE_CURRENT_SOURCE_DIR}/../standoff2/third_party/font/Roboto-Medium.ttf)
set(BIN2H ${CMAKE_CURRENT_SOURCE_DIR}/../standoff2/tools/bin2header.py)

set(FONT_H ${CMAKE_CURRENT_BINARY_DIR}/generated/roboto_font.h)
add_custom_command(
    OUTPUT ${FONT_H}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND python3 ${BIN2H} ${FONT_TTF} ${FONT_H} kFontRoboto
    DEPENDS ${FONT_TTF} ${BIN2H}
    COMMENT "font -> roboto_font.h")

add_library(ragemod_cs2 SHARED
    src/main.cpp
    src/sdk.cpp
    src/config.cpp
    src/game.cpp
    src/antiaim.cpp
    src/scan.cpp
    src/sigs.cpp
    src/ui.cpp
    src/render.cpp
    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_dx11.cpp
    ${IMGUI}/backends/imgui_impl_win32.cpp
    ${FONT_H}
)

target_include_directories(ragemod_cs2 PRIVATE src ${IMGUI} ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_compile_definitions(ragemod_cs2 PRIVATE
    IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS
    WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)

target_compile_options(ragemod_cs2 PRIVATE -O2 -fno-rtti -fno-exceptions -Wall
    -Wno-unused-parameter -Wno-unused-function -Wno-cast-function-type)

# статически линкуем рантайм: на чужой машине нет mingw-dll
target_link_options(ragemod_cs2 PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--gc-sections)

target_link_libraries(ragemod_cs2 d3d11 dxgi d3dcompiler dwmapi user32 gdi32 ole32 psapi shell32)

set_target_properties(ragemod_cs2 PROPERTIES PREFIX "" OUTPUT_NAME "ragemod_cs2")
