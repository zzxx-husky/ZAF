cmake_minimum_required(VERSION 3.5)

include_directories(.)

file(GLOB_RECURSE CoreSources src/*.cpp)
add_library(Core STATIC ${CoreSources})
