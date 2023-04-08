# library KV_STORAGE
add_library(KV_STORAGE STATIC ${STORAGE_SRC})

target_include_directories(KV_STORAGE PUBLIC ${STORAGE_DIR})

target_link_libraries(KV_STORAGE snappy)