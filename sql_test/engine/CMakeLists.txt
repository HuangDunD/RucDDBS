# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
include_directories(../../meta_server/build)
include_directories(../../meta_server)
include_directories(../build)
include_directories(../)
include_directories(../../)
aux_source_directory(. DIR_RECORD)
# 指定生成目标 
add_library(engine STATIC ${DIR_RECORD})