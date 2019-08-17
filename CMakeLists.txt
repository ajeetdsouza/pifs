cmake_minimum_required(VERSION 3.10)

project(pifs)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64 -std=c++2a -O3 -Wall -Wextra -pedantic")

include_directories(include)

add_executable(pifs src/pifs.cpp)
target_link_libraries(pifs fuse)
