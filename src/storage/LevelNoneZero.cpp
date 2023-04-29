#include <filesystem>
#include <fstream>

#include <iostream>

#include "format.h"
#include "TableBuilder.h"
#include "LevelNoneZero.h"
#include "Merger.h"

LevelNoneZero::LevelNoneZero(const std::string &dir,TableCache* table_cache, BlockCache *block_cache) : dir_(dir),
table_cache_(table_cache), block_cache_(block_cache)
{
    // if no this directory, create this
    if(!std::filesystem::exists(dir_)){
        std::filesystem::create_directories(dir_);
        size_ = 0;
        num_entries_ = 0;
        count_ = 0;
        save_meta();
    }else{
        std::ifstream ifs(dir_ + "/index", std::ios::binary);
        ifs.read((char*)&size_, sizeof(uint64_t));
        ifs.read((char*)&num_entries_, sizeof(uint64_t));
        ifs.read((char*)&count_, sizeof(uint64_t));
        for (uint64_t i = 0; i < size_; ++i) {
            uint64_t size;
            ifs.read((char*)&size, sizeof(uint64_t));
            TableMeta table_meta;
            char buf[size];
            ifs.read(buf, size);
            table_meta.DecodeFrom(std::string(buf, size));
            ssts_.emplace_back(table_meta);
        }
        ifs.close();
    }
}

void LevelNoneZero::merge(const std::vector<TableMeta> &upper_tabeles) {
    std::scoped_lock lock(latch_);

    // merge_tables
    std::vector<TableMeta> merge_tables_meta;
    for(auto sst : ssts_) {
        merge_tables_meta.push_back(sst);
    }
    for(auto sst : upper_tabeles) {
        merge_tables_meta.push_back(sst);
    }

    std::vector<std::ifstream *> merge_files;
    std::vector<SSTable *> merge_tables;
    std::vector<std::unique_ptr<Iterator>> children;

    for(int i = 0; i < merge_tables_meta.size(); i++) {
        std::ifstream *ifs = new std::ifstream(merge_tables_meta[i].table_id_.name(), std::ios::binary);
        assert(ifs->is_open());
        SSTable *table = new SSTable(ifs, nullptr);

        merge_files.push_back(ifs);
        merge_tables.push_back(table);
        children.push_back(table->NewIterator());
    }

    // 准备工作
    std::vector<TableMeta> newtables;
    
    SSTableId newtable_id(dir_, count_++);
    std::ofstream *ofs = new std::ofstream(newtable_id.name(), std::ios::binary);
    TableBuilder *table_builder = new TableBuilder(ofs);
    // 创建merger类，并正向迭代
    auto merger = NewMergingIterator(std::move(children));
    merger->SeekToFirst();
    std::string last_key = "last_key";
    while(merger->Valid()) {
        if(merger->Key() != last_key) {
            if(merger->Value() != "") {
                if(table_builder->numEntries() >= 1024) {
                    auto t_meta = table_builder->finish(newtable_id);
                    newtables.push_back(t_meta);

                    ofs->close();
                    delete table_builder; delete ofs;

                    newtable_id = SSTableId(dir_, count_++);
                    ofs = new std::ofstream(newtable_id.name(), std::ios::binary);
                    table_builder = new TableBuilder(ofs);
                }


                {
                    if(merger->Key() == "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzA")
                    {
                        auto sldkj = newtable_id;
                    }
                }
                table_builder->add(merger->Key(), merger->Value());
            }
            last_key = merger->Key();
        }       
        merger->Next();
    }

    {
        auto t_meta = table_builder->finish(newtable_id);
        newtables.push_back(t_meta);
        ofs->close();
        delete table_builder; delete ofs;
    }

    for(auto sst : ssts_) {
        std::filesystem::remove(sst.table_id_.name());
    }
    ssts_.clear();
    ssts_ = newtables;
    size_ = newtables.size();

    // 释放资源
    for(auto table : merge_tables) {
        delete table;
    }
    for(auto merge_file : merge_files) {
        delete merge_file;
    }

    save_meta();
}

// // TODO
// void LevelNoneZero::merge(const TableMeta &table_meta) {
//     // 取lhs和rhs
//     size_t lhs = 0, rhs = ssts_.size();
//     for(size_t i = 0; i < ssts_.size(); i++) {
//         if(ssts_[i].first_key_ <= table_meta.first_key_ && ssts_[i].last_key_ >= table_meta.first_key_) {
//             lhs = i;
//         }
//         if(ssts_[i].first_key_ <= table_meta.last_key_ && ssts_[i].last_key_ >= table_meta.last_key_) {
//             rhs = i + 1;
//         }
//     }
    
//     // 获得本地的SSTable
//     std::vector<std::ifstream *> ifstreams;
//     std::vector<SSTable *> sstables;
//     std::vector<std::unique_ptr<Iterator>> children;
//     for(size_t i = lhs; i < rhs; i++) {
//         std::ifstream *ifs = new std::ifstream(ssts_[i].table_id_.name(), std::ios::binary);
//         assert(ifs->is_open());
//         SSTable *table = new SSTable(ifs, nullptr);

//         ifstreams.push_back(ifs);
//         sstables.push_back(table);
//         children.push_back(table->NewIterator());
//     }
//     // 获得当前的SSTable
//     { 
//         std::ifstream *ifs = new std::ifstream(table_meta.table_id_.name(), std::ios::binary);
//         assert(ifs->is_open());
//         SSTable *table = new SSTable(ifs, nullptr);

//         ifstreams.push_back(ifs);
//         sstables.push_back(table);
//         children.push_back(table->NewIterator());
//     }

//     // 准备工作
//     std::vector<TableMeta> newtables;
    
//     SSTableId newtable_id(dir_, count_++);
//     std::ofstream *ofs = new std::ofstream(newtable_id.name(), std::ios::binary);
//     TableBuilder *table_builder = new TableBuilder(ofs);
//     // 创建merger类，并正向迭代
//     auto merger = NewMergingIterator(std::move(children));
//     merger->SeekToFirst();
//     std::string last_key = "last_key";
//     while(merger->Valid()) {
//         if(merger->Key() != last_key) {
//             if(merger->Value() != "") {
//                 if(table_builder->numEntries() > 1024) {
//                     auto t_meta = table_builder->finish(newtable_id);
//                     newtables.push_back(t_meta);

//                     newtable_id = SSTableId(dir_, count_++);
//                     delete table_builder; delete ofs;
//                     ofs = new std::ofstream(newtable_id.name(), std::ios::binary);
//                     table_builder = new TableBuilder(ofs);
//                 }
//                 table_builder->add(merger->Key(), merger->Value());
//             }
//             last_key = merger->Key();
//         }       
//         merger->Next();
//     }

//     {
//         auto t_meta = table_builder->finish(newtable_id);
//         newtables.push_back(t_meta);
//         delete table_builder; delete ofs;
//     }

//     // TODO用新的SSTables替换原来的SSTable
//     for(size_t i = lhs; i < rhs; i++) {
//         std::filesystem::remove(ssts_[i].table_id_.name());
//     }
//     ssts_.erase(ssts_.begin() + lhs, ssts_.begin() + rhs);
//     auto it = ssts_.begin(); size_t offset = lhs;
//     for(auto sst : newtables) {
//         ssts_.insert(it + offset, sst);
//         offset++;
//     }
//     // 释放资源
//     for(auto sst : sstables) {
//         delete sst;
//     }
//     for(auto ifs : ifstreams) {
//         delete ifs;
//     }

//     save_meta();
// }

TableMeta LevelNoneZero::back() const {
    return ssts_.back();
}

void LevelNoneZero::pop_back() {
    auto sstable = ssts_.back();
    ssts_.pop_back();
    std::filesystem::remove(sstable.table_id_.name());
    save_meta();
}

// TODO
std::pair<bool, std::string> LevelNoneZero::search(std::string key) {
    std::scoped_lock lock(latch_);

    for(auto sst : ssts_){
        if(key < sst.first_key_ || key > sst.last_key_){
            continue;
        }
        // TODO table cache
        std::pair<bool, std::string> result;
        if(table_cache_ != nullptr) {
            SSTable *sstable = table_cache_->open(sst.table_id_);
            result = sstable->get(key);
        }else {
            std::ifstream ifs(sst.table_id_.name(), std::ios::binary);
            SSTable sstable(&ifs, nullptr);
            result = sstable.get(key);
        }
        if(result.first){
            return result;
        }
    }
    return std::make_pair(false, "");
}

void LevelNoneZero::save_meta() const {
    std::ofstream ofs(dir_ + "/index", std::ios::binary);
    ofs.write((char*)&size_, sizeof(uint64_t));
    ofs.write((char*)&num_entries_, sizeof(uint64_t));
    ofs.write((char*)&count_, sizeof(uint64_t));
    for (auto sst : ssts_) {
        std::string buf;
        sst.EncodeInto(buf);
        uint64_t size = buf.size();
        ofs.write((char*)&(size), sizeof(uint64_t));
        ofs.write(buf.c_str(), size);
    }
    ofs.close();
}

void LevelNoneZero::clear() {
    std::scoped_lock lock(latch_);

    for(auto sst : ssts_) {
        std::filesystem::remove(sst.table_id_.name());
    }
    ssts_.clear();
    size_ = 0;

    save_meta();
}