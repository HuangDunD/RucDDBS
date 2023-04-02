#pragma once

#include <fstream>
 
#include "BlockBuilder.h"
#include "Option.h"

// #include "dbconfig.h"

class TableBuilder {
public:
    explicit TableBuilder(std::ofstream *file);

    // 禁用复制构造函数和赋值构造函数
    TableBuilder(const TableBuilder &) = delete;
    TableBuilder& operator=(const TableBuilder&) = delete;

    //
    ~TableBuilder() = default;

    // 插入键值对
    void add(const std::string &key, const std::string &value);

    void finish();

    // // 使用memtable构造一个sst文件
    // void create_sstable(const SkipList *memtable);

    // uint64_t fileSize() const;

    uint64_t numEntries() const;
private:
    std::ofstream *file_;

    BlockBuilder datablock_;
    BlockBuilder indexblock_;

    uint64_t offset_;
    uint64_t size_;
    
    std::string last_key_;

    uint64_t num_entries_;

    // 刷入磁盘
    void flush();

    void writeBlock(BlockBuilder *block);
};