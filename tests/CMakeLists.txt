cmake_minimum_required(VERSION 3.21)

include_directories(../core)

file(GLOB_RECURSE TestsSrc *.cpp)
add_executable(Tests $<TARGET_OBJECTS:CoreObject> ${TestsSrc})
target_link_libraries(Tests ${TestLibs} ${BasicLibs})
