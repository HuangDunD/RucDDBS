#include <filesystem>
#include <fstream>

#include "LevelZero.h"

LevelZero::LevelZero(const std::string &dir,TableCache* table_cache, BlockCache *block_cache) : dir_(dir),
table_cache_(table_cache), block_cache_(block_cache)
{
    // if no level0 directory, create level0
    if(!std::filesystem::exists(dir_)){
        std::filesystem::create_directories(dir_);
        size_ = 0;
        num_entries_ = 0;
        num_bytes_ = 0;
        save_meta();
    }else{
        std::ifstream ifs(dir_ + "/index", std::ios::binary);
        ifs.read((char*)&size_, sizeof(uint64_t));
        ifs.read((char*)&num_entries_, sizeof(uint64_t));
        ifs.read((char*)&num_bytes_, sizeof(uint64_t));
        for (uint64_t i = 0; i < size_; ++i) {
            uint64_t no;
            ifs.read((char*) &no, sizeof(uint64_t));
            ssts_.emplace_back(SSTableId(dir, no), table_cache_, block_cache_);
        }
        ifs.close();
    }
}

void LevelZero::add(const SkipList &memtable, uint64_t &no) {
    ssts_.emplace_back(memtable, SSTableId(dir_, no++), table_cache_, block_cache_);
    ++size_;
    num_bytes_ += memtable.space();
    num_entries_ += memtable.size();
    save_meta();
}

std::pair<bool, std::string> LevelZero::search(uint64_t key) {
    for(auto sst : ssts_){
        auto result = sst.search(key);
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
    ofs.write((char*)&num_bytes_, sizeof(uint64_t));
    for (auto sst : ssts_) {
        uint64_t no = sst.no();
        ofs.write((char*)&no, sizeof(uint64_t));
    }
    ofs.close();
}