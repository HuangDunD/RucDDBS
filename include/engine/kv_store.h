#pragma once
// 用于做交互
#include<iostream>
#include<string>
#include <memory>
#include "record.h"
#include "op_etcd.h"
#include "storage/KVStore_beta.h"

namespace kv_store{
    bool get(string key, shared_ptr<record> &val);
    bool del(string key);
    bool put(string key, shared_ptr<record> &val);
    bool get_par(string tab_name, int par, vector<shared_ptr<record>> &res);
}
extern KVStore_beta store;