# CMake 最低版本号要求
cmake_minimum_required (VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

# 项目信息
project(DistDB)

# 这个指令用于向当前工程添加存放源文件的子目录，并可以指定中间二进制和目标二进制存放的位置。
# 上面的例子定义了将 src 子目录加入工程，并指定编译输出(包含编译中间结果)路径为 bin 目录。
# 如果不进行 bin 目录的指定，那么编译结果(包括中间结果)都将存放在 build/src 目录(这个目录跟原有的 src 目录对应)
# 指定 bin 目录后，相当于在编译时将 src 重命名为 bin，所有的中间结果和目标二进制都将存放在 bin 目录。
add_subdirectory(src bin)