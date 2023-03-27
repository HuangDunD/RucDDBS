#pragma once

// 实现表中表数据的列

#include<vector>
#include<string>
#include<iostream>
using namespace std;

class column{
public:
    vector<string> column_name;
    int key_index;

    void pt_column(){
        for(auto iter: column_name){
            cout << iter << " ";
        }
    }
};