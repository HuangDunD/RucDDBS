#include <filesystem>
#include <fstream>

#include "DiskStorage.h"
#include "Option.h"

DiskStorage::DiskStorage(const std::string &dir) : dir_(dir), level0_(dir_ + "/" + Option::NAME_Z, &table_cache_, nullptr){
    if(!std::filesystem::exists(dir_)){
        std::filesystem::create_directory(dir_);
    }
    // if no meta file, create meta file
    read_meta();

    for (uint64_t i = 0; i < Option::NZ_NUM; ++i){
        auto level = std::make_unique<LevelNoneZero>(dir_ + "/" + Option::NZ_NAMES[i], &table_cache_, nullptr);
        levels_.push_back(std::move(level));
    }
}

void DiskStorage::add(const SkipList &memtable){
    std::scoped_lock lock(latch_);

    level0_.add(memtable, no_);
    no_++;
    if(level0_.size() >= Option::Z_SIZE) {
        levels_[0]->merge(level0_.sstables());
        for (uint64_t i = 0; i < Option::NZ_NUM - 1; ++i) {
            if(levels_[i]->size() >= Option::NZ_SIZE[i]) {
                levels_[i + 1]->merge(levels_[i]->sstables());
                levels_[i]->clear();
            }
        }
        level0_.clear();
    }
    // // if level0 overflows, call merger
    // if(level0_.size() > Option::Z_SIZE) {
    //     TableMeta extra = level0_.back();
    //     levels_[0]->merge(extra);
    //     for (uint64_t i = 0; i < Option::NZ_NUM - 1; ++i) {
    //         if(levels_[i]->size() > Option::NZ_SIZE[i]) {
    //             auto table = levels_[i]->back();
    //             levels_[i + 1]->merge(table);
    //             levels_[i]->pop_back();
    //         }
    //     }
    //     level0_.pop_back();
    // }  
    save_meta();
}

std::pair<bool, std::string> DiskStorage::search(std::string key) {
    std::scoped_lock lock(latch_);

    auto result = level0_.search(key);
    if(result.first == true) {
        return result;
    }
    for(auto &level : levels_) {
        auto result = level->search(key);
        if(result.first == true){
            return result;
        }
    }
    return std::make_pair(false, "");
}

void DiskStorage::read_meta() {
    if(std::filesystem::exists(std::filesystem::path(dir_ + "/meta"))){
        std::ifstream ifs(dir_ + "/meta", std::ios::binary);
        ifs.read((char*) &no_, sizeof(uint64_t));
        ifs.close();
    }else{
        no_ = 0;
        save_meta();
    }
}

void DiskStorage::save_meta() const {
    std::ofstream ofs(dir_ + "/meta", std::ios::binary);
    // byte stream
    ofs.write((char*)&no_, sizeof(uint64_t));
    ofs.close();
}