add_library(transaction STATIC ${STORAGE_SRC})

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER transaction_manager.proto)

include_directories(./build)
include_directories(../execution)
include_directories(../execution/build)
include_directories(../)
include_directories(../meta)
include_directories(../../include)
include_directories(../../meta_server/build)
include_directories(../../meta_server/)
include_directories(../../)

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

# add_executable(Lock_manager_test Lock_manager_test.cpp)

target_include_directories(KV_STORAGE PUBLIC ${STORAGE_DIR})
target_link_libraries(KV_STORAGE snappy)
