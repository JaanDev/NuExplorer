cmake_minimum_required(VERSION 3.0.0...3.5.0)
project(NuExplorer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES
    src/*.cpp
    libs/half_float/umHalf.inl
    libs/rlFPCamera/*.cpp
    libs/tinyfiledialogs/*.c
)

add_subdirectory(libs/raylib)
add_subdirectory(libs/fmt)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src libs/half_float libs/rlFPCamera libs/raygui/src libs/raygui/styles libs/tinyfiledialogs)
target_compile_definitions(${PROJECT_NAME} PRIVATE RAYGUI_IMPLEMENTATION)
target_link_libraries(${PROJECT_NAME} raylib fmt)