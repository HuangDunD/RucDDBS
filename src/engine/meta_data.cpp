#include "meta_data.h"

std::vector<std::string> ips_{"0.0.0.0:8005", "0.0.0.0:8006"};
std::unordered_map<std::string, int32_t> table_name_id_map;
std::shared_mutex table_name_id_map_mutex;

// cnt str
std::string vec_to_string(std::vector<std::string> vec){
    std::string res = std::to_string(vec.size());
    for(auto s: vec){
        res += " ";
        res += s;
    }
    return res;
}

bool create_par_table(std::string tab_name, std::shared_ptr<Column_info> &cols_info, std::vector<int> &par_vals){
    std::string str = "/meta_data/ip_address/" + tab_name;
    auto ret = etcd_get(str);
    if(ret != ""){
        std::cout << tab_name << " is exist" << std::endl;
        return 0;
    }

    std::vector<std::string> par_value;
    std::vector<std::string> par_info;
    std::vector<std::string> col_info;
    
    for(int i = 0 ; i < int(par_vals.size()) - 1; i++){
        par_value.push_back((std::to_string(i) + "," + ips_[(i%2)]));
        par_info.push_back((std::to_string(par_vals[i]) + "," + std::to_string(par_vals[i+1])));
    }

    int size = cols_info->column_name.size();
    for(int i = 0; i < size; i++){
        auto col_name = cols_info->column_name[i];
        std::string type = std::to_string(int(cols_info->column_type[i]));
        std::string len = std::to_string(4);
        col_info.push_back(col_name + "," + type + "," + len);
    }

    std::string ip_address_key = "/meta_data/ip_address/" + tab_name;
    std::string ip_address_str = vec_to_string(par_value);
    
    std::string par_info_key = "/meta_data/par_info/" + tab_name;
    std::string par_info_str = vec_to_string(par_info);
    
    std::string col_info_key = "/meta_data/col_info/" + tab_name;
    std::string col_info_str = vec_to_string(col_info);

    auto p1 = etcd_put(ip_address_key, ip_address_str);
    auto p2 = etcd_put(par_info_key, par_info_str);
    auto p3 = etcd_put(col_info_key, col_info_str);

    return p1&p2&p3;
}

bool GetColInfo(std::string tab_name, Column_info &ret){
    std::string col_info_key = "/meta_data/col_info/" + tab_name;
    std::string res = etcd_get(col_info_key);
    if(res == "")
        return false;
    
    std::stringstream ssteam;
    ssteam << res;

    int size;
    ssteam >> size;
    for(auto i = 0; i < size; i++){
        std::string tmp;
        ssteam >> tmp;
        char col_name[20];
        int type, len;
        sscanf(tmp.c_str(), "%[^,],%d,%d",col_name, &type, &len);
        // std::cout << col_name << std::endl;
        ret.column_name.push_back(col_name);
        ret.column_type.push_back(ColType(int(type)));
    }

    return true;
}

std::map<int, std::string> GetTableIp(std::string tab_name, std::string partition_key_name,
    std::string min_range, std::string max_range){
    std::map<int, std::string> res;// 分区，ip
    std::string ip_key = "/meta_data/ip_address/" + tab_name;
    std::string par_key = "/meta_data/par_info/" + tab_name;
    auto ip_val = etcd_get(ip_key);
    auto par_val = etcd_get(par_key);

    // read cnt
    std::map<int, std::string> mp;
    std::stringstream ssteam;
    ssteam << ip_val;

    int size;
    ssteam >> size;
    for(auto i = 0; i < size; i++){
        std::string tmp;
        ssteam >> tmp;
        int par;
        char ip_add[20];
        sscanf(tmp.c_str(), "%d,%s",&par, ip_add);
        mp[par] = ip_add;
    }

    ssteam.clear();
    ssteam << par_val;

    ssteam >> size;
    for(auto i = 0; i < size; i++){
        std::string tmp;
        ssteam >> tmp;
        int min, max;
        sscanf(tmp.c_str(), "%d,%d",&min, &max);
        if((atoi(min_range.c_str()) >= min && atoi(min_range.c_str()) < max) 
            || (atoi(max_range.c_str()) < max && atoi(max_range.c_str()) >= min))
            res[i] = mp[i];
    }

    return res;
}

std::vector<std::pair<int, int>> GetParRange(std::string tab_name){
    std::vector<std::pair<int, int>> res;
    std::string par_key = "/meta_data/par_info/" + tab_name;
    auto par_val = etcd_get(par_key);

    int size;
    std::stringstream ssteam;
    ssteam << par_val;

    ssteam >> size;
    for(auto i = 0; i < size; i++){
        std::string tmp;
        ssteam >> tmp;
        int min, max;
        sscanf(tmp.c_str(), "%d,%d",&min, &max);
        res.push_back(std::make_pair(min,max));
    }

    return res;
}

bool GetTables(std::vector<std::vector<std::string>> &res){
    std::string key = "/meta_data/ip_address/";
    auto ret = etcd_get_key(key);
    
    for(auto iter: ret){
        std::vector<std::string> tmp;
        tmp.push_back(iter);
        res.push_back(tmp);
    }
    return 1;
}

bool GetPartitions(std::vector<std::vector<std::string>> &res){
    std::vector<std::vector<std::string>> tables;
    GetTables(tables);
    
    for(auto table: tables){
        auto table_name = table[0];
        Column_info col;
        GetColInfo(table_name, col);
        auto mp = GetTableIp(table_name, col.column_name[0]);
        auto range = GetParRange(table_name);
        
        for(int i = 0; i < int(mp.size()); i++){
            std::vector<std::string> row;
            row.push_back(table_name);
            row.push_back(col.column_name[0]);
            row.push_back(std::to_string(i));
            row.push_back(mp[i]);
            row.push_back(std::to_string(range[i].first) + "," + std::to_string(range[i].second));
            res.push_back(row);
        }

    }
    return 0;
}