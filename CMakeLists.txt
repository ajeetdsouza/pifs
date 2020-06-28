cmake_minimum_required(VERSION 3.12)
project(pifs)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pifs src/main.cpp)
target_compile_definitions(pifs PRIVATE -D_FILE_OFFSET_BITS=64)
target_compile_options(pifs PRIVATE -Wall -Wextra -pedantic)
target_include_directories(pifs PRIVATE include)
target_link_libraries(pifs fuse3)
