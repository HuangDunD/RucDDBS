#include "kv_store.h"
KVStore_beta store("./data");
namespace kv_store{
    bool get(string key, shared_ptr<record> &val){
        std::cout << "get : " << key << std::endl;
        auto ret = etcd_get(key);
        if(ret != "")
            val->string_to_rec(ret);
        else
            return 0;
        std::cout << "str : " << ret << std::endl;
        return 1;
    }
    bool del(string key){
        std::cout << "del : " << key << std::endl;
        auto ret = etcd_del(key);
        return ret;
    }
    bool put(string key, shared_ptr<record> &val){
        std::cout << "put : " << key << std::endl;
        string str = val->rec_to_string();
        std::cout << "val : " << str << std::endl;
        
        store.put(key, str);
        auto result = store.get(key);
        std::cout << "result.first = " << result.first << ", result.second = " << result.second << std::endl;
        
        return etcd_put(key, str);
    }
    bool get_par(string tab_name, int par, vector<shared_ptr<record>> &res){
        std::cout << "get_par " << tab_name << "," << par << std::endl;
        string key = "/store_data/" + tab_name + "/" + to_string(par) + "/";
        auto ret = etcd_par(key);
        for(auto iter: ret){
            shared_ptr<record> rec(new record);
            rec->string_to_rec(iter.second);
            res.push_back(rec);
        }
        return 1;
    }
}