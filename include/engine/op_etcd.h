#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

std::string getCmdResult(const std::string &strCmd);

// 覆盖写
bool etcd_put(std::string key, std::string val);

std::string etcd_get(std::string key);

bool etcd_del(std::string key);

std::map<std::string, std::string> etcd_par(std::string key);
std::vector<std::string> etcd_get_key(std::string prefix);