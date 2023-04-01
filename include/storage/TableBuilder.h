#pragma once

#include <fstream>

#include "SkipList.h"
#include "TableId.h"
// #include "dbconfig.h"

class TableBuilder {
public:
    explicit TableBuilder(const std::ofstream *file);

    // 禁用复制构造函数和赋值构造函数
    TableBuilder(const TableBuilder &) = delete;
    TableBuilder& operator=(const TableBuilder&) = delete;

    //
    ~TableBuilder();

    // 使用memtable构造一个sst文件
    void create_sstable(const SkipList *memtable);

    uint64_t fileSize() const;

    uint64_t numEntries() const;
private:
    const std::ofstream *file_;

    uint64_t sizes_;

    uint64_t num_entries_;

    // 插入键值对
    void add(const std::string &key, const std::string &value);

    // 刷入磁盘
    void flush();
};