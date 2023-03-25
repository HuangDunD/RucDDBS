cmake_minimum_required(VERSION 3.2)

project(RUCDDBS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_FLAGS "-Wall")

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()


add_subdirectory(${PROJECT_SOURCE_DIR}/deps/snappy)
                             
add_subdirectory(${PROJECT_SOURCE_DIR}/src/storage)
add_subdirectory(${PROJECT_SOURCE_DIR}/test/storage)


