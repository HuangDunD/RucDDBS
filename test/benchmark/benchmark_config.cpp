#include "benchmark_config.h"

DEFINE_int32(BANCHMARK_NUM, 10000, "benchmark_data_num");
DEFINE_double(READ_RATIO, 0.5, "read ratio");
DEFINE_int32(THREAD_NUM, 16, "thread_num");
DEFINE_string(DIR, "./data", "data dir");
std::vector<IP_Port> NodeSet = {{"127.0.0.1",8011},{"127.0.0.1",8012},{"127.0.0.1",8013}};