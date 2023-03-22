cmake_minimum_required(VERSION 3.2)

project(RUCDDBS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_FLAGS "-Wall")

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

aux_source_directory(${PROJECT_SOURCE_DIR}/src/storage STORAGE_SRC)
set(STORAGE_DIR ${PROJECT_SOURCE_DIR}/include/storage)

add_subdirectory(${PROJECT_SOURCE_DIR}/deps/snappy)

add_executable(correctness ./test/storage/correctness.cpp ${STORAGE_SRC})

target_include_directories(correctness PUBLIC
                              ${STORAGE_DIR})

target_link_libraries(correctness snappy)                              



