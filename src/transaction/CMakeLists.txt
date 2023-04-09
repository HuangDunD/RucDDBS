cmake_minimum_required (VERSION 3.2)

project(transaction)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall")

SET(SOURCES Lock_manager.cpp)
add_library(transaction STATIC ${SOURCES})

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

include(FindThreads)
include(FindProtobuf)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER transaction_manager.proto)

# include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(./)
include_directories(../recovery)
include_directories(./build)
include_directories(../execution)
include_directories(../execution/build)
include_directories(../)
include_directories(../meta)
include_directories(../../include)
include_directories(../../meta_server/build)
include_directories(../../meta_server/)
include_directories(../../)

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

# link_directories(../meta_server/build)
# aux_source_directory(../recovery RECOVERY_SRC)

# add_executable(Lock_manager_test Lock_manager_test.cpp)
add_executable(transaction_test
    transaction_test.cpp 
    transaction_manager.cpp 
    Lock_manager.cpp 
    ../recovery/log_manager.cpp
    ../recovery/recovery_manager.cpp
    ../execution/execution_rpc.cpp
    ../execution/build/distributed_plan_service.pb.cc
    ../../meta_server/build/meta_service.pb.cc 
    ../../dbconfig.cc
    ${PROTO_SRC} ${PROTO_HEADER}
    
)

add_executable(transaction_test2
    transaction_test2.cpp 
    transaction_manager.cpp 
    Lock_manager.cpp 
    ../recovery/log_manager.cpp
    ../recovery/recovery_manager.cpp
    ../execution/execution_rpc.cpp
    ../execution/build/distributed_plan_service.pb.cc
    ../../meta_server/build/meta_service.pb.cc 
    ../../dbconfig.cc
    ${PROTO_SRC} ${PROTO_HEADER}
    
)

# target_link_libraries(Lock_manager_test transaction ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(transaction_test ${BRPC_LIB} ${DYNAMIC_LIB} ${GTEST_BOTH_LIBRARIES})
target_link_libraries(transaction_test2 ${BRPC_LIB} ${DYNAMIC_LIB} ${GTEST_BOTH_LIBRARIES})