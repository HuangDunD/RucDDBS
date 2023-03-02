//meta信息在分布式环境下存在于单个节点中, 因此需要编写rpc文件来保证
//各个分布式节点都可以访问meta server来获取表,分区的信息

//本地节点存放本地数据库,表,列的信息 见meta/local_meta.h
//meta server存放所有数据库,表,列的信息, 同时存放所有分区实时所在位置

#include "table_location.h"
#include <string>
#include <unordered_map>
#include "local_meta.h"

// template<typename T, typename = typename std::enable_if<std::is_enum<T>::value, T>::type>
// std::ostream &operator<<(std::ostream &os, const T &enum_val) {
//     os << static_cast<int>(enum_val);
//     return os;
// }

// template<typename T, typename = typename std::enable_if<std::is_enum<T>::value, T>::type>
// std::istream &operator>>(std::istream &is, T &enum_val) {
//     int int_val;
//     is >> int_val;
//     enum_val = static_cast<T>(int_val);
//     return is;
// }

struct TabMetaServer
{
    table_oid_t oid; //表id
    std::string name; //表name

    PartitionType partition_type; //分区表的分区方式
    std::string partition_key_name; //分区键列名
    int32_t partition_cnt_; //分区数
    std::vector<ParMeta> partitions; //所有分区的元信息
    PhyTableLocation table_location_; //分区位置

    // friend std::ostream &operator<<(std::ostream &os, const TabMetaServer &tab) {
    //     // TabMetaServer中有各个基本类型的变量，然后调用重载的这些变量的操作符<<
    //         os << static_cast<int32_t>(tab.oid) << ' ' << tab.name << ' ' 
    //               << tab.partition_type << ' ' << tab.partition_key_name << ' ' << tab.partition_cnt_ 
    //               << ' ' ;
    // }

    // friend std::istream &operator>>(std::istream &is, ColMeta &col) {
        
    // }
};

class DbMetaServer
{
public:
    DbMetaServer(){};
    ~DbMetaServer(){};
    DbMetaServer(std::string name,std::unordered_map<std::string, TabMetaServer*> tabs):
        name_(name), tabs_(std::move(tabs)){};
    
    inline std::unordered_map<std::string, TabMetaServer*>& gettablemap() {return tabs_;}

private:
    std::string name_; //数据库名称
    std::unordered_map<std::string, TabMetaServer*> tabs_;  // 数据库内的表名称和表元数据的映射

};

class MetaServer
{
private:
    std::unordered_map<std::string, DbMetaServer*> db_map_; //数据库名称与数据库元信息的映射
    
public:
    std::string getPartitionKey(std::string db_name, std::string table_name);
    
    void Init(){};
    MetaServer(){};
    MetaServer(std::unordered_map<std::string, DbMetaServer*> db_map):db_map_(std::move(db_map)){};
    ~MetaServer(){};
};
