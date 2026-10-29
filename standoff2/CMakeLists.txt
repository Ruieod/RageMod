cmake_minimum_required(VERSION 3.22)
project(ragemod CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

# шрифт меню вшиваем в либу: на телефоне читать его неоткуда
set(FONT_H ${CMAKE_CURRENT_BINARY_DIR}/generated/roboto_font.h)
add_custom_command(
    OUTPUT ${FONT_H}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/bin2header.py
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/font/Roboto-Medium.ttf
            ${FONT_H} kFontRoboto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/font/Roboto-Medium.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/bin2header.py
    COMMENT "font -> roboto_font.h")

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
    ${FONT_H}
)

target_include_directories(ragemod PRIVATE src ${IMGUI}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zygisk
    ${CMAKE_CURRENT_BINARY_DIR}/generated)

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
