# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_RECORD)
# 指定生成目标 
add_library(kv STATIC ${DIR_RECORD})