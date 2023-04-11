# RucDDBS

**2023/04/09** 添加KVStore_beta.cpp。因为KVStore包含了事务部分的代码，现在编译有些问题，为了测试存储部分的功能，我将事务部分代码分离出去形成KVStore_beta.cpp，支持key为std::string类型的接口

**使用KVStore_beta**

1. **包含头文件KVStore_beta.h**: include "KVStore_beta.h"
2. **创建KVStore_beta对象**: KVStore_beta store("datapath");
3. **在cmake项目中链接KV_STORAGE_BETA库**:  target_link_libraries(storage_example KV_STORAGE_BETA)

transaction test

./transaction_test -SERVER_NAME server1 -SERVER_LISTEN_PORT 8002 -log_path /home/t500ttt/RucDDBS/data/
./transaction_test2 -SERVER_NAME server2 -SERVER_LISTEN_PORT 8003 -log_path /home/t500ttt/RucDDBS/data2/