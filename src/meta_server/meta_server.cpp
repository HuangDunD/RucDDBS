#include "meta_server.h"
#include <iostream>
#include <type_traits>

std::string MetaServer::getPartitionKey(std::string db_name, std::string table_name){
    DbMetaServer *dms = db_map_[db_name];
    if(dms == nullptr) 
        throw MetaServerError::NO_DATABASE;
    TabMetaServer *tms = dms->gettablemap()[table_name];
    if(tms == nullptr)
        throw MetaServerError::NO_TABLE;
    return tms->partition_key_name;
}

// template <typename T>
std::unordered_map<partition_id_t,ReplicaLocation> MetaServer::getReplicaLocationList
            (std::string db_name, std::string table_name, std::string partitionKeyName, int64_t min_range, int64_t max_range){

    DbMetaServer *dms = db_map_[db_name];
    if(dms == nullptr) 
        throw MetaServerError::NO_DATABASE;
    TabMetaServer *tms = dms->gettablemap()[table_name];
    if(tms == nullptr)
        throw MetaServerError::NO_TABLE;
    if(partitionKeyName != tms->partition_key_name)
        throw MetaServerError::PARTITION_KEY_NOT_TRUE;
    
    if(tms->partition_type == PartitionType::RANGE_PARTITION){
        return getReplicaLocationListByRange(tms, min_range, max_range);
    }
    else if(tms->partition_type == PartitionType::HASH_PARTITION){
        return getReplicaLocationListByHash(tms, min_range, max_range);
    }
    else{
        std::unordered_map<partition_id_t,ReplicaLocation> res;
        for(auto par : tms->partitions){
            res[par.p_id] = *tms->table_location_.getReplicaLocation(par.p_id);
        }
        return res;
    }
}

// template <typename T>
std::unordered_map<partition_id_t,ReplicaLocation> MetaServer::getReplicaLocationListByRange 
            (TabMetaServer *tms, int64_t min_range, int64_t max_range){
    
    std::unordered_map<partition_id_t,ReplicaLocation> res;
    if(std::is_same<int64_t, int64_t>::value == true){
        for(auto par : tms->partitions){
            if(par.int_range.min_range >= min_range
                && par.int_range.max_range <= max_range){
                res[par.p_id] = *tms->table_location_.getReplicaLocation(par.p_id);
            }
        }
    }
    // else if(std::is_same<T, std::string>::value == true){
    //     for(auto par : tms->partitions){
    //         if(par.string_range.min_range >= min_range
    //             && par.string_range.max_range <= max_range){
    //             res[par.p_id] = *tms->table_location_.getReplicaLocation(par.p_id);
    //         }
    //     }
    // }
    return res;
}

// template <typename T>
std::unordered_map<partition_id_t,ReplicaLocation>  MetaServer::getReplicaLocationListByHash 
            (TabMetaServer *tms, int64_t min_range, int64_t max_range){
    //这里暂时先简化 由于Hash分区均匀分区的特征 对于一般的范围的查询 
    //极大概率是对于所有的分区都有分区涉及 因此可以不进行Hash计算直接返回所有分区
    //而对于等值查询可以直接定位到分区

    std::unordered_map<partition_id_t,ReplicaLocation> res;

    if(max_range == min_range){
        //等值查询
        partition_id_t p_id = tms->HashPartition(min_range);
        res[p_id] = *tms->table_location_.getReplicaLocation(p_id);
    }else{
        //范围查询
        for(auto par : tms->partitions){
            res[par.p_id] = *tms->table_location_.getReplicaLocation(par.p_id);
        }
    }

    return res;

}