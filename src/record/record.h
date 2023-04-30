#pragma once

// 实现表中每一行数据结构体
// 与存储层交互

#include<vector>
#include<string>
#include<iostream>

#include "nlohmann/json.hpp"

using namespace std;

// 每一个单元的数据类型
enum value_type{
    integer,
    character
};

class value{
public:
    // 构造函数
    value(value_type t, string c, string t, int s) : type(t), col_name(c), tab_name(t), str(s) {

    }

    value_type type;
    string col_name;
    string tab_name;
    int str;
};

class record{
public:
    vector<shared_ptr<value>> row;

    void pt_row(){
        for(auto iter: row){
            cout << iter->str << " ";
        }
    }

    void encodeInto(std::string &value) const {
        using json = nlohmann::json;
        
        json j;
        for(auto iter :row) {
            j[iter->col_name]["value_type"] = iter->type;
            j[iter->col_name]["col_name"] = iter->col_name;
            j[iter->col_name]["tab_name"] = iter->tab_name;
            j[iter->col_name]["str"] = iter->str;
        }
        value = j.dump();
    }

    void decodeFrom(const std::string &s) {
        using json = nlohmann::json;

        json j = json::parse(s);
        for(json::iterator it = j.begin(); it != j.end(); ++it) {
            row.push_back(std::make_shared<value>((*it)["value_type"], (*it)["col_name"], (*it)["tab_name"], (*it)["str"]));
        }
    }
};