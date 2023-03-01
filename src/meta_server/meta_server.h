//meta信息在分布式环境下存在于单个节点中, 因此需要编写rpc文件来保证
//各个分布式节点都可以访问meta server来获取表,分区的信息

//本地节点存放本地数据库,表,列的信息 见meta/local_meta.h
//meta server存放所有数据库,表,列的信息, 同时存放所有分区实时所在位置

#include "table_location.h"
#include <string>
#include <unordered_map>
#include "local_meta.h"

struct TabMetaServer
{
    table_oid_t oid; //表id
    std::string name; //表name

    PartitionType partition_type; //分区表的分区方式
    int32_t partition_cnt_; //分区数
    std::vector<ParMeta> partitions; //所有分区的元信息
    PhyTableLocation table_location_; //分区位置
};

class DbMetaServer
{
private:
    std::string name_; //数据库名称
    std::unordered_map<std::string, TabMetaServer> tabs_;  // 数据库内的表名称和表元数据的映射
public:
    DbMetaServer(/* args */);
    ~DbMetaServer();
};

class MetaServer
{
private:
    DbMetaServer db_;
    
public:
    MetaServer(){};
    ~MetaServer(){};
};
