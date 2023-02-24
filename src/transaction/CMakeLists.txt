cmake_minimum_required (VERSION 3.2)

project(test-lock_manager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall")

SET(SOURCES Lock_manager.cpp)
add_library(transaction STATIC ${SOURCES})

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(Lock_manager_test Lock_manager_test.cpp)

target_link_libraries(Lock_manager_test transaction ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
