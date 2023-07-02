# RucDDBS

**2023/04/09** 添加KVStore_beta.cpp。因为KVStore包含了事务部分的代码，现在编译有些问题，为了测试存储部分的功能，我将事务部分代码分离出去形成KVStore_beta.cpp，支持key为std::string类型的接口

**使用KVStore_beta**

1. **包含头文件KVStore_beta.h**: include "KVStore_beta.h"
2. **创建KVStore_beta对象**: KVStore_beta store("datapath");
3. **在cmake项目中链接KV_STORAGE_BETA库**:  target_link_libraries(storage_example KV_STORAGE_BETA)

# transaction test
./transaction_test -SERVER_NAME server1 -SERVER_LISTEN_PORT 8002 -log_path /home/t500ttt/RucDDBS/data/
./transaction_test2 -SERVER_NAME server2 -SERVER_LISTEN_PORT 8003 -log_path /home/t500ttt/RucDDBS/data2/

# transaction benchmark test
# 生成负载
./benchmark_workload -DIR ./data1
./benchmark_workload -DIR ./data2
./benchmark_workload -DIR ./data3

# 数据检验
./data_test -DIR ./data1

# 负载测试
./benchmark_test -DIR ./data1 -SERVER_NAME server1 -SERVER_LISTEN_PORT 8011 -log_path /home/t500ttt/RucDDBS/data1/ -THREAD_NUM 16
./benchmark_test -DIR ./data2 -SERVER_NAME server2 -SERVER_LISTEN_PORT 8012 -log_path /home/t500ttt/RucDDBS/data2/ -THREAD_NUM 16
./benchmark_test -DIR ./data3 -SERVER_NAME server3 -SERVER_LISTEN_PORT 8013 -log_path /home/t500ttt/RucDDBS/data3/ -THREAD_NUM 16

./benchmark_test -DIR ./data1 -SERVER_NAME server1 -SERVER_LISTEN_PORT 8011 -log_path /root/RucDDBS/data1/ -META_SERVER_ADDR 192.168.0.188:8001 -THREAD_NUM 16 -stack_size_normal=10000000 -tc_stack_normal=1 -READ_RATIO=1 -OP_MAX_NUM=30 -BANCHMARK_NUM 100000 -OP_Theta 0.2
./benchmark_test -DIR ./data2 -SERVER_NAME server2 -SERVER_LISTEN_PORT 8012 -log_path /root/RucDDBS/data2/ -META_SERVER_ADDR 192.168.0.188:8001 -THREAD_NUM 16 -stack_size_normal=10000000 -tc_stack_normal=1 -READ_RATIO=1 -OP_MAX_NUM=30 -BANCHMARK_NUM 100000
./benchmark_test -DIR ./data3 -SERVER_NAME server3 -SERVER_LISTEN_PORT 8013 -log_path /root/RucDDBS/data3/ -META_SERVER_ADDR 192.168.0.188:8001 -THREAD_NUM 16 -stack_size_normal=10000000 -tc_stack_normal=1 -READ_RATIO=1 -OP_MAX_NUM=30 -BANCHMARK_NUM 100000
