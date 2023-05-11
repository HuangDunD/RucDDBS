# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
include_directories(../)
include_directories(../build)
set(DIR_SRCS engine.cpp kv_store.cpp meta_data.cpp op_etcd.cpp)
# 指定生成目标 
add_library(engine STATIC ${DIR_SRCS})
target_link_libraries(engine)