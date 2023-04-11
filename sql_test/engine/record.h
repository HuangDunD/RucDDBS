#pragma once

// 实现表中每一行数据结构体
// 与存储层交互

#include<vector>
#include<string>
#include<iostream>
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
};