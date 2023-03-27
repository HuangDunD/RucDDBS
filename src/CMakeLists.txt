include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_SRCS)
# 添加子目录
add_subdirectory(parser)
add_subdirectory(record)
add_subdirectory(KV)
add_subdirectory(node)
# 指定生成目标 
add_executable(demo ${DIR_SRCS})
# 添加链接库
target_link_libraries(demo parser record kv node)