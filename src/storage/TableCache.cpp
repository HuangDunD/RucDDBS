#include "TableCache.h"
#include "Option.h"


TableCache::~TableCache() {
    for(auto it : lists_){
        it.second.ifs_->close();
        delete it.second.sstable_;
        delete it.second.ifs_;
    }
}

SSTable *TableCache::open(SSTableId id) {
    // in map, return ifstream directly, and update the position in list
    if(map_.count(id.no_)){
        // std::cout << "TableCache: " << "Cache命中, tableid = " << id.no_ << std::endl;
        lists_.push_front(*map_[id.no_]);
        lists_.erase(map_[id.no_]);
        map_[id.no_] = lists_.begin();
        return map_[id.no_]->second.sstable_;
    }
    if(lists_.size() == Option::TABLE_CACHE_SIZE){
        // std::cout << "TableCache: " << "移除tableid = " << lists_.back().first << std::endl;
        // not in map and full
        map_.erase(lists_.back().first);
        lists_.back().second.ifs_->close();
        delete lists_.back().second.sstable_;
        delete lists_.back().second.ifs_;
        lists_.pop_back();
    }
    // std::cout << "TableCache: " << "读入tableid = " << id.no_ << std::endl;
    std::ifstream *ifs = new std::ifstream(id.name(), std::ios::binary);
    SSTable *sstable = new SSTable(ifs, nullptr);
    lists_.emplace_front(id.no_, file_and_table{ifs, sstable});
    map_[id.no_] = lists_.begin();
    return sstable;   
}

void TableCache::close(SSTableId id) {
    if(!map_.count(id.no_)){
        return;
    }
    map_[id.no_]->second.ifs_->close();
    delete map_[id.no_]->second.sstable_;
    delete map_[id.no_]->second.ifs_;
    lists_.erase(map_[id.no_]);
    map_.erase(id.no_);
}