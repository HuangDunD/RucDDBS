#include "benchmark_config.h"

DEFINE_int32(BANCHMARK_NUM, 10000, "benchmark_data_num");
DEFINE_double(READ_RATIO, 0.5, "read ratio");
DEFINE_int32(THREAD_NUM, 1, "thread_num");
DEFINE_string(DIR, "./data2", "data dir");
DEFINE_int32(OP_MAX_NUM, 30, "max ops num in a txn");

std::vector<IP_Port> NodeSet = {{"127.0.0.1",8011},{"127.0.0.1",8012},{"127.0.0.1",8013}};
// std::vector<IP_Port> NodeSet = {{"192.168.0.85",8011},{"192.168.0.226",8012},{"192.168.0.56",8013}};