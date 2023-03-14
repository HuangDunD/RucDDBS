//meta信息在分布式环境下存在于单个节点中, 因此需要编写rpc文件来保证
//各个分布式节点都可以访问meta server来获取表,分区的信息

//本地节点存放本地数据库,表,列的信息 见meta/local_meta.h
//meta server存放所有数据库,表,列的信息, 同时存放所有分区实时所在位置

#include "table_location.h"
#include "local_meta.h"
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <unordered_map>
#include <shared_mutex>

static const std::string META_SERVER_FILE_NAME = "META_SERVER.meta";
enum class MetaServerError {
    NO_DATABASE,
    NO_TABLE,
    PARTITION_KEY_NOT_TRUE,
    PARTITION_TYPE_NOT_TRUE,
    NO_PARTITION_OR_REPLICA,
    NO_META_DIR,
    UnixError,
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
        case MetaServerError::NO_META_DIR:
            return "no meta server dir found";
        case MetaServerError::UnixError: 
            return "cd meta server dir error";
    }

    // Todo: Should fail with unreachable.
    return "";
  }
    ~MetaServerErrorException(){};
};

struct Node{
    std::string ip_addr;
    int32_t port;
    bool activate;
    int32_t leader_par_cnt;
    int32_t follower_par_cnt;
    Node(){}
    Node(std::string _ip_addr, int32_t _port, bool _activate){
        ip_addr = _ip_addr;
        port = _port;
        activate = _activate;
        leader_par_cnt = 0;
        follower_par_cnt = 0;
    }
    // 重载操作符 <<
    friend std::ostream &operator<<(std::ostream &os, const Node &node) {
        os << node.ip_addr << " " << node.port << " " << node.activate << " " << node.leader_par_cnt <<
            " " << node.follower_par_cnt << '\n';
        return os;
    }
    // 重载操作符 >>
    friend std::istream &operator>>(std::istream &is, Node &node) {
        is >> node.ip_addr >> node.port >> node.activate >> node.leader_par_cnt 
            >> node.follower_par_cnt;
        return is;
    }
};

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
    std::shared_mutex mutex_; 
    partition_id_t HashPartition(int64_t val){
        return std::hash<int64_t>()(val) % partition_cnt_;
    }
    partition_id_t HashPartition(std::string val){
        return std::hash<std::string>()(val) % partition_cnt_;
    }

    friend std::ostream &operator<<(std::ostream &os, const TabMetaServer &tab) {
        // TabMetaServer中有各个基本类型的变量，然后调用重载的这些变量的操作符<<
        os << tab.oid << ' ' << tab.name << ' ' << tab.partition_type << ' ' 
            << tab.partition_key_name << ' ' << tab.partition_key_type << ' ' << tab.partition_cnt_  << ' ' << 
            tab.partitions.size() << '\n';
        for (auto &entry : tab.partitions){
            os << entry;
        }
        os << tab.table_location_;
        return os;
    }

    friend std::istream &operator>>(std::istream &is, TabMetaServer &tab) {
        size_t n;
        is >> tab.oid >> tab.name >> tab.partition_type >> tab.partition_key_name 
            >> tab.partition_key_type >> tab.partition_cnt_  >> n;
        for(int i=0; i<n; i++){
            ParMeta pm;
            is >> pm;
            tab.partitions.push_back(pm);
        }
        is >> tab.table_location_;
        return is;
    }
};

class DbMetaServer
{
public:
    DbMetaServer(){next_oid_ = 0;};
    ~DbMetaServer(){};
    DbMetaServer(std::string name):name_(name){next_oid_ = 0;};
    DbMetaServer(std::string name,std::unordered_map<std::string, TabMetaServer*> tabs):
        name_(name), tabs_(std::move(tabs)){next_oid_ = 0;};
    
    inline std::unordered_map<std::string, TabMetaServer*>& gettablemap() {return tabs_;}
    inline std::string get_db_name() {return name_; }
    inline table_oid_t get_next_oid() {return next_oid_;}
    inline void set_next_oid(table_oid_t next_oid) { next_oid_ = next_oid;}
    // 重载操作符 <<
    friend std::ostream &operator<<(std::ostream &os, const DbMetaServer &db_meta_server) {
        os << db_meta_server.next_oid_ << " " << db_meta_server.name_ << " " << db_meta_server.tabs_.size() << '\n' ;
        for (auto &entry : db_meta_server.tabs_) {
            os << *entry.second << '\n'; //entry.second是TabMetaServer, 然后调用重载的TableMetaServer的操作符<<
        }
        return os;
    }
    // 重载操作符 >>
    friend std::istream &operator>>(std::istream &is, DbMetaServer &db_meta_server) {
        is >> db_meta_server.next_oid_ >> db_meta_server.name_;
        size_t n;
        is >> n;
        for(int i=0; i<n; i++){
            auto tms = new TabMetaServer();
            is >> *tms;
            db_meta_server.tabs_[tms->name] = tms;
        }
        return is;
    }
    inline std::shared_mutex& get_mutex(){return mutex_;}
private:
    table_oid_t next_oid_; //分配表oid
    std::string name_; //数据库名称
    std::unordered_map<std::string, TabMetaServer*> tabs_;  // 数据库内的表名称和表元数据的映射
    std::shared_mutex mutex_; 
};

class MetaServer
{
private:
    std::unordered_map<std::string, DbMetaServer*> db_map_; //数据库名称与数据库元信息的映射
    std::unordered_map<std::string, Node*> ip_node_map_;
    std::shared_mutex mutex_; 

public:
    inline std::shared_mutex& get_mutex(){return mutex_;}
    inline const std::unordered_map<std::string, DbMetaServer*>& get_db_map() {return db_map_;}
    inline std::unordered_map<std::string, DbMetaServer*>& mutable_db_map() {return db_map_;}
    inline const std::unordered_map<std::string, Node*>& get_ip_node_map() {return ip_node_map_;}
    inline std::unordered_map<std::string, Node*>& mutable_ip_node_map() {return ip_node_map_;}

    std::string getPartitionKey(std::string db_name, std::string table_name);

    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationList( std::string db_name, std::string table_name, 
            std::string partitionKeyName, std::string min_range, std::string max_range);

    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationListByRange (TabMetaServer *tms, std::string min_range, std::string max_range);

    std::unordered_map<partition_id_t,ReplicaLocation> getReplicaLocationListByHash (TabMetaServer *tms, std::string min_range, std::string max_range);

    bool UpdatePartitionLeader(std::string db_name, std::string tab_name, partition_id_t p_id, std::string ip_addr);

    bool is_dir(const std::string &meta_name) {
        struct stat st;
        return stat(meta_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    void open_meta_server(const std::string &meta_name);

    void close_meta_server(const std::string &meta_name);
    
    // 重载操作符 <<
    friend std::ostream &operator<<(std::ostream &os, const MetaServer &meta_server) {
        os << meta_server.db_map_.size() << '\n';
        for (auto &entry : meta_server.db_map_) {
            os << *entry.second << '\n';  // entry.second是DbMetaServer类型，然后调用重载的DbMetaServer的操作符<<
        }
        os << meta_server.ip_node_map_.size() << '\n';
        for (auto &entry : meta_server.ip_node_map_) {
            os << *entry.second << '\n';  // entry.second是Node类型，然后调用重载的Node的操作符<<
        }
        return os;
    }
    // 重载操作符 >>
    friend std::istream &operator>>(std::istream &is, MetaServer &meta_server) {
        size_t n;
        is >> n;
        for (size_t i = 0; i < n; i++) {
            auto db_meta_server = new DbMetaServer();
            is >> *db_meta_server;
            meta_server.db_map_[db_meta_server->get_db_name()] = db_meta_server;
        }
        is >> n;
        for (size_t i = 0; i < n; i++) {
            auto node = new Node();
            is >> *node;
            meta_server.ip_node_map_[node->ip_addr] = node;
        }
        return is;
    }
    void Init(){};
    MetaServer(){};
    MetaServer(std::unordered_map<std::string, DbMetaServer*> db_map):db_map_(std::move(db_map)){};
    ~MetaServer(){};
};



