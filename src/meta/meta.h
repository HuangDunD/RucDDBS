#include <string>
#include <vector>
#include <unordered_map>
#include "table_location.h"



enum ColType {
    TYPE_INT, TYPE_FLOAT, TYPE_STRING
};

struct ColMeta {
    std::string tab_name;  // 字段所属表名称
    std::string name;      // 字段名称
    ColType type;          // 字段类型
    int len;               // 字段长度
    int offset;            // 字段位于记录中的偏移量
    bool index;            // 该字段上是否建立索引
};


struct TabMeta {
    table_oid_t oid;
    std::string name;
    std::vector<ColMeta> cols;
    PhyTableLocation* table_location; 

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
