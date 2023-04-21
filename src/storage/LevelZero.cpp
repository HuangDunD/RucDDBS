#include <filesystem>
#include <fstream>

#include <iostream>

#include "format.h"
#include "TableBuilder.h"
#include "LevelZero.h"

LevelZero::LevelZero(const std::string &dir,TableCache* table_cache, BlockCache *block_cache) : dir_(dir),
table_cache_(table_cache), block_cache_(block_cache)
{
    // if no level0 directory, create level0
    if(!std::filesystem::exists(dir_)){
        std::filesystem::create_directories(dir_);
        size_ = 0;
        num_entries_ = 0;
        save_meta();
    }else{
        std::ifstream ifs(dir_ + "/index", std::ios::binary);
        ifs.read((char*)&size_, sizeof(uint64_t));
        ifs.read((char*)&num_entries_, sizeof(uint64_t));
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

void LevelZero::add(const SkipList &memtable, uint64_t no) {
    // ssts_.emplace_back(SSTableId(dir_, no));
    SSTableId new_table_id(dir_, no);
    TableMeta new_table_meta = TableBuilder::create_sstable(memtable, new_table_id);
    ssts_.push_back(new_table_meta);
    ++size_;
    num_entries_ += memtable.size();
    save_meta();
}

// TODO
std::pair<bool, std::string> LevelZero::search(std::string key) {
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

void LevelZero::save_meta() const {
    std::ofstream ofs(dir_ + "/index", std::ios::binary);
    ofs.write((char*)&size_, sizeof(uint64_t));
    ofs.write((char*)&num_entries_, sizeof(uint64_t));
    for (auto sst : ssts_) {
        std::string buf;
        sst.EncodeInto(buf);
        uint64_t size = buf.size();
        ofs.write((char*)&(size), sizeof(uint64_t));
        ofs.write(buf.c_str(), size);
    }
    ofs.close();
}