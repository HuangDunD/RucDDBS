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

std::unordered_map<partition_id_t,ReplicaLocation> MetaServer::getReplicaLocationList
            (std::string db_name, std::string table_name, std::string partitionKeyName, std::string min_range, std::string max_range){

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
            auto rep_ptr = tms->table_location_.getReplicaLocation(par.p_id);
            if(rep_ptr == nullptr)
                throw MetaServerError::NO_PARTITION_OR_REPLICA;
            res[par.p_id] = *rep_ptr;
        }
        return res;
    }
}

std::unordered_map<partition_id_t,ReplicaLocation> MetaServer::getReplicaLocationListByRange ( 
    TabMetaServer *tms, std::string min_range, std::string max_range ){
    
    std::unordered_map<partition_id_t,ReplicaLocation> res;
    if(tms->partition_key_type == ColType::TYPE_INT || tms->partition_key_type == ColType::TYPE_FLOAT){
        for(auto par : tms->partitions){
            if(stoi(par.string_range.min_range) <= stoi(max_range)
                && stoi(par.string_range.max_range) >= stoi(min_range)){

                auto rep_ptr = tms->table_location_.getReplicaLocation(par.p_id);
                if(rep_ptr == nullptr)
                    throw MetaServerError::NO_PARTITION_OR_REPLICA;
                res[par.p_id] = *rep_ptr;
            }
        }
    }
    else if(tms->partition_key_type == ColType::TYPE_STRING){
        for(auto par : tms->partitions){
            if(par.string_range.min_range <= max_range
                && par.string_range.max_range >= min_range){

                auto rep_ptr = tms->table_location_.getReplicaLocation(par.p_id);
                if(rep_ptr == nullptr)
                    throw MetaServerError::NO_PARTITION_OR_REPLICA;
                res[par.p_id] = *rep_ptr;
            }
        }
    }

    return res;
}

std::unordered_map<partition_id_t,ReplicaLocation>  MetaServer::getReplicaLocationListByHash 
            (TabMetaServer *tms, std::string min_range, std::string max_range){
    //这里暂时先简化 由于Hash分区均匀分区的特征 对于一般的范围的查询 
    //极大概率是对于所有的分区都有分区涉及 因此可以不进行Hash计算直接返回所有分区
    //而对于等值查询可以直接定位到分区

    std::unordered_map<partition_id_t,ReplicaLocation> res;
    if(tms->partition_key_type == ColType::TYPE_INT || tms->partition_key_type == ColType::TYPE_FLOAT){
        int int_min_range = stoi(min_range);
        int int_max_range = stoi(max_range);

        if(int_max_range == int_min_range){
            //等值查询
            partition_id_t p_id = tms->HashPartition(int_min_range);
            auto rep_ptr = tms->table_location_.getReplicaLocation(p_id);
            if(rep_ptr == nullptr)
                throw MetaServerError::NO_PARTITION_OR_REPLICA;
            res[p_id] = *rep_ptr;
        }else{
            //范围查询
            for(auto par : tms->partitions){
                auto rep_ptr = tms->table_location_.getReplicaLocation(par.p_id);
                if(rep_ptr == nullptr)
                    throw MetaServerError::NO_PARTITION_OR_REPLICA;
                res[par.p_id] = *rep_ptr;
            }
        }
        return res;
    }
    else if(tms->partition_key_type == ColType::TYPE_STRING){
        if(max_range == min_range){
            //等值查询
            partition_id_t p_id = tms->HashPartition(min_range);
            auto rep_ptr = tms->table_location_.getReplicaLocation(p_id);
            if(rep_ptr == nullptr)
                throw MetaServerError::NO_PARTITION_OR_REPLICA;
            res[p_id] = *rep_ptr;
        }else{
            //范围查询
            for(auto par : tms->partitions){
                auto rep_ptr = tms->table_location_.getReplicaLocation(par.p_id);
                if(rep_ptr == nullptr)
                    throw MetaServerError::NO_PARTITION_OR_REPLICA;
                res[par.p_id] = *rep_ptr;
            }
        }

        return res;
    }

    return res;
}