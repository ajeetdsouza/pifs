cmake_minimum_required(VERSION 3.10)

project(tracy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -O3 -Wall -Wextra -pedantic")

include_directories(include)

add_executable(main src/main.cpp)
