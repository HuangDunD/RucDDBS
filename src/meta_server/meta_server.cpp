#include "meta_server.h"

std::string MetaServer::getPartitionKey(std::string db_name, std::string table_name){
    return db_map_[db_name]->gettablemap()[table_name]->partition_key_name;
}

