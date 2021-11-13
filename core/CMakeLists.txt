cmake_minimum_required(VERSION 3.5)

include_directories(.)

file(GLOB_RECURSE CoreHeaders zaf/*.hpp)
file(GLOB_RECURSE CoreSources src/*.cpp)
add_library(CoreObject OBJECT ${CoreSources})
set_property(TARGET CoreObject PROPERTY POSITION_INDEPENDENT_CODE 1)

set(CoreSources ${CoreSources} PARENT_SCOPE)
set(CoreHeaders ${CoreHeaders} PARENT_SCOPE)
