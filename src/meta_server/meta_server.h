//meta信息在分布式环境下存在于单个节点中, 因此需要编写rpc文件来保证
//各个分布式节点都可以访问meta server来获取表,分区的信息

//本地节点存放本地数据库,表,列的信息, 同时存放本地表各个分区所在位置的Cache
//meta server存放所有数据库,表,列的信息, 同时存放所有分区实时所在位置

#include "table_location.h"

class MetaServer_DB_Info
{
private:
    PhyTableLocation table_location_;
    PhyTableLocation table_location_; 
public:
    MetaServer_DB_Info();
    ~MetaServer_DB_Info();
};


class MetaServer
{
private:
    /* data */
public:
    MetaServer(/* args */);
    ~MetaServer();
};

MetaServer::MetaServer(/* args */)
{
}

MetaServer::~MetaServer()
{
}
