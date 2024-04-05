cmake_minimum_required(VERSION 3.5)

project(imgui-overlay-test)

add_executable(
  ${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(
  ${PROJECT_NAME}
    imgui-overlay
)