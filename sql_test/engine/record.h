#pragma once

// 实现表中每一行数据结构体
// 与存储层交互

#include<vector>
#include<string>
#include<iostream>
#include <sstream> 
using namespace std;

// 每一个单元的数据类型
enum value_type{
    integer,
    character
};

class value{
public:
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
    string rec_to_string(){
        string res = to_string(row.size());
        // 只存值，列由上层处理
        for(auto iter: row){
            res += " ";
            res += to_string(iter->str);
        }

        return res;
    }

    void string_to_rec(string in_str){
        stringstream str;
        str << in_str;

        int size;
        str >> size;
        for(int i = 0; i < size; i++){
            int out;
            str >> out;
            shared_ptr<value> val(new value);
            val->str = out;
            row.push_back(val);
        }
    }
};