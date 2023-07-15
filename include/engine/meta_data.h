#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <sstream>
#include "op_etcd.h"
#include <shared_mutex>
#define par_min -2147483648
#define par_max 2147483647

extern std::vector<std::string> ips_;
extern std::unordered_map<std::string, int32_t> table_name_id_map;
extern std::shared_mutex table_name_id_map_mutex;

enum class ColType {
    TYPE_INT, TYPE_FLOAT, TYPE_STRING
};

struct Column_info{
    std::vector<std::string> column_name;
    std::vector<ColType> column_type;    
};

// cnt str
std::string vec_to_string(std::vector<std::string> vec);

bool create_par_table(std::string tab_name, std::shared_ptr<Column_info>& cols_info, std::vector<int>& par_vals);

bool GetColInfo(std::string tab_name, Column_info &ret);

std::map<int, std::string> GetTableIp(std::string tab_name, std::string partition_key_name,
     std::string min_range = "-10000", std::string max_range = "10000");

bool GetTables(std::vector<std::vector<std::string>> &res);
bool GetPartitions(std::vector<std::vector<std::string>> &res);