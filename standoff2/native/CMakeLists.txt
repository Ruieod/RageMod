cmake_minimum_required(VERSION 3.18)
project(ragemod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# cfg_gen.h is produced by tools/gen_cfg.py (CI and tools/build.sh run it first)
set(GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/gen/cpp" CACHE PATH "directory containing cfg_gen.h")
if(NOT EXISTS "${GEN_DIR}/cfg_gen.h")
    message(FATAL_ERROR "cfg_gen.h missing in ${GEN_DIR} - run tools/gen_cfg.py")
endif()

add_library(ragemod SHARED
    src/main.cpp src/common.cpp src/il2cpp.cpp src/unity.cpp src/game.cpp src/visuals.cpp
    src/aim.cpp src/bridge.cpp src/jni_bridge.cpp)
target_include_directories(ragemod PRIVATE src ${GEN_DIR})
target_compile_options(ragemod PRIVATE -fexceptions -frtti -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
target_link_libraries(ragemod PRIVATE log dl)
# libc++ must be shared-linked like every other library in the process, and no export surface other than JNI_OnLoad
target_link_options(ragemod PRIVATE -Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384)
