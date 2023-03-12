#include <string>
#include <vector>
#include <unordered_map>
// #include "table_location.h"
using table_oid_t = int32_t;
using partition_id_t = int32_t;
enum class ColType {
    TYPE_INT, TYPE_FLOAT, TYPE_STRING
};

enum class PartitionType{
    NONE_PARTITION, //非分区表
    RANGE_PARTITION, //RANGE分区
    HASH_PARTITION  //HASH分区
};

struct ColMeta {
    table_oid_t oid;       // 字段所属表id
    std::string name;      // 字段名称
    ColType type;          // 字段类型
    int len;               // 字段长度
    int offset;            // 字段位于记录中的偏移量
    bool index;            // 该字段上是否建立索引
};

struct ParMeta {
    std::string name; //分区名
    partition_id_t p_id; //分区id
    //此处应该有一个共用体(Union)存放分区的范围 

    // struct {
    //     int64_t min_range;
    //     int64_t max_range;
    // }int_range;

    struct {
        std::string min_range;
        std::string max_range;
    }string_range;

    friend std::ostream &operator<<(std::ostream &os, const ParMeta &pm) {
        os << pm.name << ' ' << pm.p_id << ' ' << pm.string_range.min_range << ' ' << pm.string_range.max_range << '\n';
        return os;
    }

    friend std::istream &operator>>(std::istream &is, ParMeta &pm) {
        is >> pm.name >> pm.p_id >> pm.string_range.min_range >> pm.string_range.max_range;
        return is;
    }
};

struct TabMeta {
    table_oid_t oid;
    std::string name;
    std::vector<ColMeta> cols;

    PartitionType partition_type; //分区表的分区方式
    std::string partition_key_name; //分区键列名
    ColType partition_key_type; //分区列属性
    std::vector<ParMeta> partitions;

    //TODO
    bool is_col(const std::string &col_name) const ;
    std::vector<ColMeta>::iterator get_col(const std::string &col_name) const;

};

class DbMeta {

private:
    std::string name_;                     // 数据库名称
    std::unordered_map<std::string, TabMeta> tabs_;  // 数据库内的表名称和表元数据的映射

public:
    // DbMeta(std::string name) : name_(name) {}

    bool is_table(const std::string &tab_name) const { return tabs_.find(tab_name) != tabs_.end(); }

    TabMeta &get_table(const std::string &tab_name) {
        auto pos = tabs_.find(tab_name);
        if (pos == tabs_.end()) {
            throw 0;
        }
        return pos->second;
    }


};
