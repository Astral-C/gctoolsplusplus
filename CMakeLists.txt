cmake_minimum_required(VERSION 3.8)
project(gctools++)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include lib/bStream)

file(GLOB_RECURSE GCTOOLSPLUS_SRC
    "src/*.cpp"
    "src/*.c"
    "include/*.hpp"
    "include/*.h"
)

add_library(gctools++ STATIC ${GCTOOLSPLUS_SRC})


add_executable(gctools++_test test/main.cpp)
target_link_libraries(gctools++_test gctools++)