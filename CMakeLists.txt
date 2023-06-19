cmake_minimum_required(VERSION 3.2)

project(RUCDDBS)

option(RUCDDBS_STORAGE "if build kv_storage" ON)
option(RUCDDBS_TRANSACTION "if build transaction" OFF)
option(RUCDDBS_RAFT "if build raft" ON)
option(RUCDDBS_TEST "build rucddbs test" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_FLAGS "-Wall -ggdb -O0")

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

# find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
# include_directories(${GTEST_INCLUDE_DIRS})

find_library(THRIFT_LIB NAMES thrift)
if (NOT THRIFT_LIB)
    set(THRIFT_LIB "")
endif()
find_library(THRIFTNB_LIB NAMES thriftnb)
if (NOT THRIFTNB_LIB)
    set(THRIFTNB_LIB "")
endif()

find_path(BRPC_INCLUDE_PATH NAMES brpc/server.h)
if(LINK_SO)
    find_library(BRPC_LIB NAMES brpc)
else()
    find_library(BRPC_LIB NAMES libbrpc.a brpc)
endif()
if((NOT BRPC_INCLUDE_PATH) OR (NOT BRPC_LIB))
    message(FATAL_ERROR "Fail to find brpc")
endif()
include_directories(${BRPC_INCLUDE_PATH})

find_path(GFLAGS_INCLUDE_PATH gflags/gflags.h)
find_library(GFLAGS_LIBRARY NAMES gflags libgflags)
if((NOT GFLAGS_INCLUDE_PATH) OR (NOT GFLAGS_LIBRARY))
    message(FATAL_ERROR "Fail to find gflags")
endif()
include_directories(${GFLAGS_INCLUDE_PATH})

find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif()
include_directories(${LEVELDB_INCLUDE_PATH})

# find_path(JACKSON_INCLUDE_PATH NAMES jackson/Value.h)
# message(${JACKSON_INCLUDE_PATH})
# find_library(JACKSON_LIB NAMES jackson)
# if ((NOT JACKSON_INCLUDE_PATH) OR (NOT JACKSON_LIB))
#     message(FATAL_ERROR "Fail to find jackson")
# endif()
# include_directories(${JACKSON_INCLUDE_PATH})

find_package(OpenSSL)
include_directories(${OPENSSL_INCLUDE_DIR})

set(DYNAMIC_LIB
    ${CMAKE_THREAD_LIBS_INIT}
    ${GFLAGS_LIBRARY}
    ${PROTOBUF_LIBRARIES}
    ${LEVELDB_LIB}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${OPENSSL_SSL_LIBRARY}
    ${THRIFT_LIB}
    ${THRIFTNB_LIB}
    dl
    )


include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/meta_server/)
include_directories(${PROJECT_SOURCE_DIR}/meta_server/build)



set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include/)
set(STORAGE_DIR ${PROJECT_SOURCE_DIR}/include/storage/)
set(TRANSACTION_DIR ${PROJECT_SOURCE_DIR}/include/transaction/)
set(IMMEMORY_DIR ${PROJECT_SOURCE_DIR}/include/Inmemory/)
set(RECOVERY_DIR ${PROJECT_SOURCE_DIR}/include/recovery/)
set(RAFT_DIR ${PROJECT_SOURCE_DIR}/include/raft/)
set(EXECUTION_DIR ${PROJECT_SOURCE_DIR}/src/execution)
set(ENGINE_DIR ${PROJECT_SOURCE_DIR}/include/engine/)
set(PARSER_DIR ${PROJECT_SOURCE_DIR}/include/parser/)

add_library(config STATIC dbconfig.cc) 
aux_source_directory(${PROJECT_SOURCE_DIR}/src/storage STORAGE_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/transaction TRANSACTION_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/recovery RECOVERY_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/raft RAFT_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/execution EXECUTION_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/engine ENGINE_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/parser PARSER_SRC)
# 添加依赖
add_subdirectory(${PROJECT_SOURCE_DIR}/deps/snappy)
# add_subdirectory(${PROJECT_SOURCE_DIR}/deps/jackson)
# add_subdirectory(${PROJECT_SOURCE_DIR}/deps/tinyev)
add_subdirectory(${PROJECT_SOURCE_DIR}/deps/jrpc)
add_subdirectory(${PROJECT_SOURCE_DIR}/include/proto)

IF(RUCDDBS_TEST)
  message("build ructest")
  add_subdirectory(${PROJECT_SOURCE_DIR}/deps/googletest)
  add_subdirectory(${PROJECT_SOURCE_DIR}/test/storage)
  add_subdirectory(${PROJECT_SOURCE_DIR}/test/recovery)
  add_subdirectory(${PROJECT_SOURCE_DIR}/test/transaction)
  add_subdirectory(${PROJECT_SOURCE_DIR}/test/benchmark)
endif()

if(RUCDDBS_STORAGE)
  message("build rucddbs storage")
  add_subdirectory(${PROJECT_SOURCE_DIR}/src/storage)

  add_executable(storage_example ${PROJECT_SOURCE_DIR}/storage_example.cpp)
  target_link_libraries(storage_example KV_STORAGE_BETA)
endif()

if(RUCDDBS_TRANSACTION)
  add_subdirectory(${PROJECT_SOURCE_DIR}/src/recovery)
  add_subdirectory(${PROJECT_SOURCE_DIR}/src/transaction)
endif()

if(RUCDDBS_RAFT)
  message("build rucddbs store with raft")
  # include_directories(
  #     deps/jrpc
  #     deps/jrpc/3rd/jackson
  #     deps/jrpc/3rd/tinyev
  #     /include/raft
  #     ${PROJECT_SOURCE_DIR}
  # ${PROJECT_BINARY_DIR}
  # )
  # add_subdirectory(deps/jrpc)
  # add_subdirectory(${PROJECT_SOURCE_DIR}/src/raft)
  
  add_executable(raftTest ${PROJECT_SOURCE_DIR}/raftTest.cpp)
  add_executable(raftTest2 ${PROJECT_SOURCE_DIR}/raftTest2.cpp)
  target_link_libraries(raftTest RAFT_BETA)
  target_link_libraries(raftTest2 RAFT_BETA)
  # add_executable(raftExample ${PROJECT_SOURCE_DIR}/raftExample.cpp)
  # target_link_libraries(raftExample RAFT_EXAMPLE)

  set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
  set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

  include_directories(
          raft
          deps/jrpc
          deps/jrpc/3rd/tinyev
          deps/jrpc/3rd/jackson
          ${PROJECT_SOURCE_DIR}
          ${PROJECT_BINARY_DIR})
  add_subdirectory(${PROJECT_SOURCE_DIR}/src/raft)

endif()

message("build rucddbs engine & store")
# add_subdirectory(${PROJECT_SOURCE_DIR}/src/storage)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/engine)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/parser)
# add_executable(storage_example ${PROJECT_SOURCE_DIR}/storage_example.cpp)
add_executable(client ${PROJECT_SOURCE_DIR}/client.cpp)
add_executable(server1 ${PROJECT_SOURCE_DIR}/server1.cpp)
add_executable(server2 ${PROJECT_SOURCE_DIR}/server2.cpp)

# target_link_libraries(storage_example KV_STORAGE_BETA)
target_link_libraries(client proto engine parser)
target_link_libraries(server1 proto engine parser)
target_link_libraries(server2 proto engine parser)
