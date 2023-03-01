cmake_minimum_required (VERSION 3.2)

project(meta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall")

# SET(SOURCES meta.cpp)
# add_library(meta STATIC ${SOURCES})

add_executable(meta meta.cpp)

target_link_libraries(meta)