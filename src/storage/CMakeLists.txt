# library KV_STORAGE
add_library(KV_STORAGE STATIC ${STORAGE_SRC})

# include transaction

set(transaction_dir ${PROJECT_SOURCE_DIR}/src/transaction)
set(recovery_dir ${PROJECT_SOURCE_DIR}/src/recovery)

target_include_directories(KV_STORAGE PUBLIC ${transaction_dir} ${recovery_dir})

target_include_directories(KV_STORAGE PUBLIC ${STORAGE_DIR})

target_link_libraries(KV_STORAGE snappy)