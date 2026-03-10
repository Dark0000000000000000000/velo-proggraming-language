cmake_minimum_required(VERSION 3.16)
project(vegame)
set(CMAKE_CXX_STANDARD 17)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Audio)

add_library(vegame SHARED vegame.cpp)

target_include_directories(vegame PRIVATE ../../src)

target_link_libraries(vegame PRIVATE
    SFML::Graphics
    SFML::Audio
)

set_target_properties(vegame PROPERTIES
    PREFIX ""
    OUTPUT_NAME "vegame"
)

# Copy vegame.dll next to velo.exe after build
add_custom_command(TARGET vegame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:vegame>
        ${CMAKE_BINARY_DIR}/../../build/vegame.dll
    COMMENT "Copying vegame.dll -> build/"
)