#pragma once
#include "head.h"

class KV{
public:
    vector<shared_ptr<record>> records;
    
    vector<int> Key_range(){
        vector<int> res{0,1};
        return res;
    }
    void get(int key, shared_ptr<record>& res){
        res = records[key];
    }
};
