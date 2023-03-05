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
    ColType partition_key_type; //分区列属性
    partition_id_t partition_cnt_; //分区数
    std::vector<ParMeta> partitions; //所有分区的元信息
    PhyTableLocation table_location_; //分区位置

    partition_id_t HashPartition(int64_t val){
        return std::hash<int64_t>()(val) % partition_cnt_;
    }
    partition_id_t HashPartition(std::string val){
        return std::hash<std::string>()(val) % partition_cnt_;
    }

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

    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationList(std::string db_name, std::string table_name, 
            std::string partitionKeyName, int64_t min_range, int64_t max_range);

    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationList( std::string db_name, std::string table_name, 
            std::string partitionKeyName, std::string min_range, std::string max_range);

    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationListByRange (TabMetaServer *tms, int64_t min_range, int64_t max_range);
    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationListByRange (TabMetaServer *tms, std::string min_range, std::string max_range);

    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationListByHash (TabMetaServer *tms, int64_t min_range, int64_t max_range);
    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationListByHash (TabMetaServer *tms, std::string min_range, std::string max_range);

    void Init(){};
    MetaServer(){};
    MetaServer(std::unordered_map<std::string, DbMetaServer*> db_map):db_map_(std::move(db_map)){};
    ~MetaServer(){};
};

enum class MetaServerError {
    NO_DATABASE,
    NO_TABLE,
    PARTITION_KEY_NOT_TRUE,
    PARTITION_TYPE_NOT_TRUE,
    NO_PARTITION_OR_REPLICA
};

class MetaServerErrorException : public std::exception
{
private:
    MetaServerError err_;
public:
    explicit MetaServerErrorException(MetaServerError err):err_(err){}
    MetaServerError getMetaServerError(){return err_;}
    std::string GetInfo() {
    switch (err_) {
        case MetaServerError::NO_DATABASE:
            return "there isn't this database in MetaServer";
        case MetaServerError::NO_TABLE:
            return "there isn't this table in MetaServer";
        case MetaServerError::PARTITION_KEY_NOT_TRUE:
            return "request's partitition key is different from Metaserver's";
        case MetaServerError::PARTITION_TYPE_NOT_TRUE:
            return "request's partitition type is different from Metaserver's";
        case MetaServerError::NO_PARTITION_OR_REPLICA:
            return "there isn't partition or replica required in MetaServer";
    }

    // Todo: Should fail with unreachable.
    return "";
  }
    ~MetaServerErrorException();
};

