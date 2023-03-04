#include "meta_server.h"
#include <iostream>

std::string MetaServer::getPartitionKey(std::string db_name, std::string table_name){
    DbMetaServer *dms = db_map_[db_name];
    if(dms == nullptr) 
        throw MetaServerError::NO_DATABASE;
    TabMetaServer *tms = dms->gettablemap()[table_name];
    if(tms == nullptr)
        throw MetaServerError::NO_TABLE;
    return tms->partition_key_name;
}

std::vector<ReplicaLocation> MetaServer::getReplicaLocationListByRange (std::string db_name, std::string table_name, 
            std::string partitionKeyName, int64_t min_range, int64_t max_range){
    
    DbMetaServer *dms = db_map_[db_name];
    if(dms == nullptr) 
        throw MetaServerError::NO_DATABASE;
    TabMetaServer *tms = dms->gettablemap()[table_name];
    if(tms == nullptr)
        throw MetaServerError::NO_TABLE;
    if(partitionKeyName != tms->partition_key_name)
        throw MetaServerError::PARTITION_KEY_NOT_TRUE;
    if(tms->partition_type != PartitionType::RANGE_PARTITION)
        throw MetaServerError::PARTITION_TYPE_NOT_TRUE;
    
    std::vector<ReplicaLocation> res;

    for(auto par : tms->partitions){
        if(par.partition_val.range_partition.min_range >= min_range
              && par.partition_val.range_partition.max_range <= max_range){
            res.push_back(*tms->table_location_.getReplicaLocation(par.p_id));
        }
    }
    return res;
}

std::vector<ReplicaLocation> MetaServer::getReplicaLocationListByHash (std::string db_name, std::string table_name, 
            std::string partitionKeyName, int64_t min_range, int64_t max_range){

    DbMetaServer *dms = db_map_[db_name];
    if(dms == nullptr) 
        throw MetaServerError::NO_DATABASE;
    TabMetaServer *tms = dms->gettablemap()[table_name];
    if(tms == nullptr)
        throw MetaServerError::NO_TABLE;
    if(partitionKeyName != tms->partition_key_name)
        throw MetaServerError::PARTITION_KEY_NOT_TRUE;
    if(tms->partition_type != PartitionType::RANGE_PARTITION)
        throw MetaServerError::PARTITION_TYPE_NOT_TRUE;

    std::vector<ReplicaLocation> res;

    //这里暂时先简化 由于Hash分区均匀分区的特征 对于一般的范围的查询 
    //极大概率是对于所有的分区都有分区涉及 因此可以不进行计算直接返回所有分区
    //而对于等值查询可以直接定位到分区
    if(max_range == min_range){
        //等值查询
        partition_id_t p_id = tms->HashPartition(min_range);
        res.push_back(*tms->table_location_.getReplicaLocation(p_id));
    }else{
        //范围查询
        for(auto par : tms->partitions){
            res.push_back(*tms->table_location_.getReplicaLocation(par.p_id));
        }
    }

    return res;
    
}