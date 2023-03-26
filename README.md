# RucDDBS
[2023/03/21] 增加了存储部分，现在支持点查询，接口定义在`include/storage/KVStore.h`文件中

[2023/03/24] 纯悦反馈`key`值应该用`std::string`，不应该用`uint_64t`，否则在设计关系数据到KV的映射时不好设计，加入后续修改（改动这个需要修改`SSTable`文件的设计，现在打算先加入测试部分，看看存储系统整体的逻辑有没有问题）

[2023/03/24] 纯悦反馈应该为`KVStore`应该加一个flush函数，便于直接刷盘(本周内添加)

[2023/03/25] 发现一个编译问题，`snappy`依赖中包含了`googletest`依赖，和原来的`googletest`依赖重复，直接编译会报错，错误为`redefine gtest`等，解决的措施如下：

1. 进入`deps/snappy`目录下，打开`CMakeLists.txt`
2. 查找变量`SNAPPY_BUILD_TEST`，将下面的语句中的`ON`修改为`OFF`
3. 然后删除原来的`build`文件夹，重新编译即可（`SNAPPY_BUILD_BENCHMARK`对本项目没有作用，也可以修改为`OFF`)

[2023/03/26] 为`storage`部分加入了测试文件`correct_gtest`，基于`googletest`，测试内容为三部分：

1. 第一部分为`single_test`，单个键值对的增删改查
2. 第二部分为`small_test`，顺序的增删改查1024个键值对
3. 第三部分为`large_test`，顺序的增删改查1024 * 64个键值对

目前，在前两个测试没有发现错误，但是发现在`large_test`时，会出现`std::bad_alloc`错误，初步分析为内存分配错误，需要再完善一下代码
