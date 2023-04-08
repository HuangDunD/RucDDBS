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

# 定义变量
set(STORAGE_DIR ${PROJECT_SOURCE_DIR}/include/storage/)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/storage STORAGE_SRC)

# 添加依赖
add_subdirectory(${PROJECT_SOURCE_DIR}/deps/snappy)

IF(RUCDDBS_TEST)
  message("build ructest")
  add_subdirectory(${PROJECT_SOURCE_DIR}/deps/googletest)
  add_subdirectory(${PROJECT_SOURCE_DIR}/test/storage)
endif()

if(RUCDDBS_STORAGE)
  message("build rucddbs storage")
  add_subdirectory(${PROJECT_SOURCE_DIR}/src/storage)
endif()




