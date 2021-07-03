cmake_minimum_required(VERSION 3.5)

include_directories(../core)

set(ZAF Core)

macro(add symbol file)
  add_executable(${symbol} ${file})
  target_link_libraries(${symbol} ${ZAF} ${BasicLibs})
endmacro()

add(HelloWorld hello_world.cpp)
add(Shuffle shuffle.cpp)
