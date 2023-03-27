#pragma once
#include "head.h"
using namespace std;

class Node{
public:
    string node_name;
    shared_ptr<KV> store;
    Node(){
        store.reset(new KV);
    }
};