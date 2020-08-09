cmake_minimum_required(VERSION 3.8)
project(te)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)
add_executable(te te.cpp src/screen.cpp child.cpp src/font_cache.cpp)
target_link_libraries(te PUBLIC SDL2 SDL2_ttf SDL2_image)