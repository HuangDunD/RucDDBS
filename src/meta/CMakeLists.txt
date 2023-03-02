cmake_minimum_required (VERSION 3.2)

project(meta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall")

include_directories(.)
include_directories(../meta_server/)
include_directories(../meta_server/build)
# include(meta_service.pb.h)

# SET(SOURCES meta.cpp)
# add_library(meta STATIC ${SOURCES})

include(FindThreads)
include(FindProtobuf)

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
    
add_executable(meta meta.cpp)
# include_directories()
# add_executable(meta_client meta_client.cpp)

target_link_libraries(meta)
# link_directories(../meta_server/build)
# target_link_libraries(meta_client meta_server ${BRPC_LIB} ${DYNAMIC_LIB} pthread)