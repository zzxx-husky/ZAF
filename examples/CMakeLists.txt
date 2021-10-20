cmake_minimum_required(VERSION 3.5)

include_directories(../core)

set(ZAF Core)

macro(add symbol file)
  add_executable(${symbol} ${file})
  target_link_libraries(${symbol} ${ZAF} ${BasicLibs})
endmacro()

add(HelloWorld hello_world.cpp)
add(HelloWorldX hello_world_x.cpp)
add(HelloWorldByActorEngine hello_world_by_actor_engine.cpp)
add(HelloWorldNetwork hello_world_network.cpp)
add(ActorDelivery actor_delivery.cpp)
add(LoadRebalance load_rebalance.cpp)
add(Shuffle shuffle.cpp)
add(ShuffleX shufflex.cpp)
add(PrintMacros print_macros.cpp)
