cmake_minimum_required (VERSION 3.2)

project(recovery)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall")

add_executable(recovery log_manager.cpp)

target_link_libraries(recovery)