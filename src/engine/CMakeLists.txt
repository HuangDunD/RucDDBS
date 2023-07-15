set(ENGINE_DIR ${PROJECT_SOURCE_DIR}/include/engine/)
set(PARSER_DIR ${PROJECT_SOURCE_DIR}/include/parser/)
set(DIR_SRCS engine.cpp kv_store.cpp meta_data.cpp op_etcd.cpp)
# 指定生成目标 
add_library(engine STATIC ${DIR_SRCS})
target_include_directories(engine PUBLIC ${ENGINE_DIR} ${PARSER_DIR} ${TRANSACTION_DIR})
target_link_libraries(engine proto KV_STORAGE_BETA transaction ${BRPC_LIB} ${DYNAMIC_LIB})
