cmake_minimum_required(VERSION 3.15)

project(imgui-overlay)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)

set(SDL_STATIC ON CACHE BOOL "Build a SDL static library (if available)")
set(SDL_SHARED OFF CACHE BOOL "Build a SDL shared library (if available)")

add_subdirectory(extern/SDL)

add_library(
    ${PROJECT_NAME}
      extern/imgui/imgui.cpp
      extern/imgui/imgui_draw.cpp
      extern/imgui/imgui_widgets.cpp
      extern/imgui/imgui_tables.cpp
      extern/imgui/imgui_demo.cpp
      extern/imgui/backends/imgui_impl_sdl3.cpp
      extern/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(
    ${PROJECT_NAME}
      PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        extern/imgui
        extern/imgui/backends
)

target_link_libraries(
    ${PROJECT_NAME}
      PUBLIC
        SDL3::SDL3
)

add_subdirectory(test)