#include "local_meta.h"

//DBManager管理本地数据库, 表, 分区, 索引等元信息, 
//一个数据库对应一个DBManager 
class DBManager
{
public:
    DbMeta db_;

public:
    DBManager(){};
    ~DBManager(){};

    // Database management
    bool is_dir(const std::string &db_name);

    void create_db(const std::string &db_name);

    void drop_db(const std::string &db_name);

    void open_db(const std::string &db_name);

    void close_db();


};

