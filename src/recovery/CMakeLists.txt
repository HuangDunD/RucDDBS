cmake_minimum_required (VERSION 3.2)

project(recovery)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(log_manager_test log_manager_test.cpp log_manager.cpp)

target_link_libraries(log_manager_test ${CMAKE_THREAD_LIBS_INIT} ${GTEST_BOTH_LIBRARIES})