cmake_minimum_required(VERSION 3.16)
project(velo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)

# ── GLFW ──────────────────────────────────────────────────────────────────────
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    find_library(GLFW_LIB glfw3 glfw)
    find_path(GLFW_INC GLFW/glfw3.h)
    add_library(glfw3_lib INTERFACE)
    target_include_directories(glfw3_lib INTERFACE ${GLFW_INC})
    target_link_libraries(glfw3_lib INTERFACE ${GLFW_LIB})
    add_library(glfw ALIAS glfw3_lib)
endif()

# ── Dear ImGui (vendored) ─────────────────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    OpenGL::GL
    glfw
)

# ── Velo executable ───────────────────────────────────────────────────────────
add_executable(velo src/main.cpp)

target_include_directories(velo PRIVATE src)

target_link_libraries(velo PRIVATE imgui OpenGL::GL glfw)

if(WIN32)
    target_link_libraries(velo PRIVATE gdi32 imm32)
endif()

# Optimize
target_compile_options(velo PRIVATE -O2)

# ── Extensions ────────────────────────────────────────────────────────────────
add_subdirectory(extensions/vegame)
