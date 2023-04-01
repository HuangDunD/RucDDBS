cmake_minimum_required(VERSION 3.2)

project(RUCDDBS)

option(RUCDDBS_STORAGE "if build kv_storage" OFF)
option(RUCDDBS_TEST "build rucddbs test" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_FLAGS "-Wall")

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

set(STORAGE_DIR ${PROJECT_SOURCE_DIR}/include/storage/)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/storage STORAGE_SRC)

IF(RUCDDBS_TEST)
  # add_subdirectory(${PROJECT_SOURCE_DIR}/deps/googletest)
  # add_executable(skiplist_gtest 
  #                       ${PROJECT_SOURCE_DIR}/test/storage/skiplist_gtest.cpp 
  #                       ${PROJECT_SOURCE_DIR}/src/storage/SkipList.cpp
  #                       ${PROJECT_SOURCE_DIR}/src/storage/Entry.cpp)
  # target_include_directories(skiplist_gtest PRIVATE ${STORAGE_DIR})
  # target_link_libraries(skiplist_gtest gmock_main gtest)
  message("build ructest")
  add_subdirectory(${PROJECT_SOURCE_DIR}/deps/googletest)
  add_subdirectory(${PROJECT_SOURCE_DIR}/test/storage)
endif()

if(RUCDDBS_STORAGE)
  add_subdirectory(${PROJECT_SOURCE_DIR}/deps/snappy)
  add_subdirectory(${PROJECT_SOURCE_DIR}/src/storage)
endif()




