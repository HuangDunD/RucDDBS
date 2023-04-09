// 用于做交互
#include<iostream>
#include<string>
#include <memory>
#include "record.h"

bool get(string key, shared_ptr<record> val){
    //
    std::cout << "get : " << key << std::endl;

}

bool del(string key){
    std::cout << "del : " << key << std::endl;
}

bool put(string key, shared_ptr<record> val){
    std::cout << "put : " << key << std::endl;
}

bool get_par(string tab_name, int par, vector<shared_ptr<record>> &ret){
    std::cout << "get_par " << tab_name << "," << par << std::endl;
}