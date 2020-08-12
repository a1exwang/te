cmake_minimum_required(VERSION 3.8)
project(te)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)
add_executable(te te.cpp
        src/subprocess.cpp
        src/display.cpp
        src/font_cache.cpp
        src/screen.cpp
        src/tty_input.cpp
        )
target_link_libraries(te PUBLIC SDL2 SDL2_ttf SDL2_image)

add_executable(tailf tailf.cpp)