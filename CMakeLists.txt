cmake_minimum_required(VERSION 3.8)
project(te)
add_executable(te te.cpp screen.cpp)
target_link_libraries(te PUBLIC SDL2 SDL2_ttf)