#include <cassert>
#include <string>
#include <cstring>

#include <iostream>

#include "SSTable.h"
#include "snappy.h"

SSTable::SSTable(std::ifstream *ifs, BlockCache *block_cache) 
                    : ifs_(ifs), block_cache_(block_cache) {
    assert(ifs_->is_open());
    ifs_->seekg(- sizeof(Footer), std::ios::end);
    char buf[sizeof(Footer)];
    ifs_->read(buf, sizeof(Footer));
    footer_.DecodeFrom(std::string(buf, sizeof(Footer)));


    BlockContents index_content;
    ReadBlock(ifs, footer_.index_handle(), &index_content);

    index_block_ = new Block(index_content);

    index_iter_ = index_block_->NewIterator();
    // uint64_t index_offset, index_size;
    // ifs_->seekg(- sizeof(uint64_t) * 2, std::ios::end);
    // ifs_->read((char*)&index_offset, sizeof(uint64_t));
    // ifs_->read((char*)&index_size, sizeof(uint64_t));
    
    // index_block_ = loadBlock(ifs_, index_offset, index_size);
    // index_iter_ = index_block_->NewIterator();
}

SSTable::~SSTable() {
    // std::cout << __FILE__ << __LINE__ <<  "" << std::endl;
    delete index_block_;
}

// std::unique_ptr<Block> SSTable::loadBlock(std::ifstream *ifs, uint64_t block_offset, uint64_t block_size){
//     assert(ifs->is_open());
//     char buffer[block_size];
//     ifs->seekg(block_offset, std::ios::beg);
//     ifs->read(buffer, block_size);
    
//     std::string uncompressed;
//     if(Option::BLOCK_COMPRESSED) {
//         snappy::Uncompress(buffer, block_size, &uncompressed);
//     }
//     return std::make_unique<Block>(uncompressed);
// }

std::pair<bool, std::string> SSTable::get(const std::string &key) const {
    assert(ifs_->is_open());
    
    auto iter = NewIterator();
    iter->Seek(key);
    if(iter->Valid() && iter->Key() == key) {
        return std::make_pair(true, iter->Value());
    }
    return std::make_pair(false, "");
    // // 首先找到在哪个block中
    // index_iter_->SeekToFirst();
    // while(index_iter_->Valid()) {
    //     std::string last_key = index_iter_->Key();
    //     // 如果last_key >= key，说明可能在这个block里
    //     if(last_key >= key) {
    //         // 计算block位置的大小
    //         BlockHandle data_handle;
    //         data_handle.DecodeFrom(index_iter_->Value());

    //         // 是否实现了block_cache
    //         if(block_cache_ == nullptr) {
    //             // 没有实现block_cache，直接读取    
    //             BlockContents data_content;
    //             ReadBlock(ifs_, data_handle, &data_content);            
    //             std::unique_ptr<Block> data_block = std::make_unique<Block>(data_content);
    //             auto ret = data_block->get(key);
    //             return ret;
    //         }else {
                
    //         }
    //     }else {
    //         // 否则查看下一个block
    //         index_iter_->Next();
    //     }
    // }
    // return std::make_pair(false, "");
}

std::unique_ptr<Iterator> SSTable::NewIterator() const {
    return std::make_unique<SSTable::Iter>(this);
}

SSTable::Iter::Iter(const SSTable *sstable) : sstable_(sstable), ifs_(sstable->ifs_), block_cache_(sstable_->block_cache_),
                                            index_block_(sstable->index_block_), index_iter_(index_block_->NewIterator()), data_block_(nullptr) {
}

SSTable::Iter::~Iter() {
    if(block_cache_ == nullptr && data_block_ != nullptr) {
        delete data_block_;
    }
}

bool SSTable::Iter::Valid() const {
    return index_iter_->Valid() && data_iter_->Valid();
}

void SSTable::Iter::SeekToFirst() {
    std::scoped_lock lock(latch_);
    index_iter_->SeekToFirst();
    read_from_index();
    if(data_block_ != nullptr) {
        data_iter_->SeekToFirst();
    }
}

void SSTable::Iter::SeekToLast() {
    std::scoped_lock lock(latch_);
    index_iter_->SeekToLast();
    read_from_index();
    if(data_block_ != nullptr) {
        data_iter_->SeekToLast();
    }
}

void SSTable::Iter::Seek(const std::string &key) {
    std::scoped_lock lock(latch_);
    index_iter_->Seek(key);
    read_from_index();
    if(data_block_ != nullptr) {
        data_iter_->Seek(key);
    }
}

void SSTable::Iter::Next() {
    std::scoped_lock lock(latch_);
    assert(Valid());
    data_iter_->Next();
    if(!data_iter_->Valid()) {
        index_iter_->Next();
        read_from_index();
        if(data_block_ != nullptr) {
            data_iter_->SeekToFirst();
        }
    }
}

void SSTable::Iter::Prev() {
    std::scoped_lock lock(latch_);
    assert(Valid());
    data_iter_->Prev();
    if(!data_iter_->Valid()) {
        index_iter_->Prev();
        read_from_index();
        if(data_block_ != nullptr) {
            data_iter_->SeekToLast();
        }
    }
}

std::string SSTable::Iter::Key() const {
    assert(Valid());
    return data_iter_->Key();
}

std::string SSTable::Iter::Value() const {
    assert(Valid());
    return data_iter_->Value();
}

// 根据当前的index读入对应的block，如果index不合法，则data_block为nullptr
void SSTable::Iter::read_from_index() {
    // 如果没有缓冲池，则先释放内存
    if(block_cache_ == nullptr && data_block_ != nullptr) {
        delete data_block_;
    }
    if(!index_iter_->Valid()){
        data_block_ = nullptr;
        data_iter_ = nullptr;
        return ;
    }
    BlockHandle data_handle;
    data_handle.DecodeFrom(index_iter_->Value());
    if(block_cache_ == nullptr) {
        BlockContents data_contents;
        ReadBlock(ifs_, data_handle, &data_contents);
        data_block_ = new Block(data_contents);
        data_iter_ = data_block_->NewIterator();
    }else {

    }
    return ;
}








// #include <fstream>

// #include "SSTable.h"
// #include "snappy.h"

// SSTable::SSTable(const SSTableId &id, TableCache *table_cache, BlockCache *block_cache) 
//                 : id_(id), table_cache_(table_cache), block_cache_(block_cache){
//     std::ifstream ifs(id_.name(), std::ios::binary);
//     ifs.read((char*)&num_entries_, sizeof(uint64_t));
//     for(uint64_t i = 0; i <= num_entries_; i++){
//         uint64_t key;
//         uint64_t offset;
//         ifs.read((char*)&key, sizeof(uint64_t));
//         ifs.read((char*)&offset, sizeof(uint64_t));
//         keys_.push_back(key);
//         offsets_.push_back(offset);
//     }
//     ifs.read((char*)&num_blocks_, sizeof(uint64_t));
//     for(uint64_t i = 0; i <= num_blocks_; i++){
//         uint64_t ori, cmp;
//         ifs.read((char*)&ori, sizeof(uint64_t));
//         ifs.read((char*)&cmp, sizeof(uint64_t));
//         oris_.push_back(ori);
//         cmps_.push_back(cmp);
//     }
//     ifs.close();
// }

// SSTable::SSTable(const SkipList &memtable, const SSTableId &id, TableCache *table_cache, BlockCache *block_cache) 
//                 : id_(id), table_cache_(table_cache), block_cache_(block_cache)
// {
//     num_entries_ = memtable.size();

//     std::string block;
//     block.reserve(Option::BLOCK_SPACE);

//     std::string blockseq;
//     blockseq.reserve(Option::SSTABLE_SPACE);

//     SkipList::Iterator it = memtable.iterator();

//     num_blocks_ = 0;
//     uint64_t offset = 0;
//     uint64_t ori = 0;
//     uint64_t cmp = 0;
//     uint64_t entryInBlockCnt = 0;

//     while(it.hasNext()){
//         Entry entry = it.next();
//         keys_.push_back(entry.key_);
//         offsets_.push_back(offset);
//         offset += entry.value_.size();
//         block += entry.value_;
//         entryInBlockCnt++;
//         if(block.size() >= Option::BLOCK_SPACE){
//             std::string compressed;
//             if (Option::BLOCK_COMPRESSED)
//                 snappy::Compress(block.data(), block.size(), &compressed);
//             else
//                 compressed = block;
//             blockseq += compressed;
//             oris_.push_back(ori);
//             cmps_.push_back(cmp);
//             block.clear();
//             entryInBlockCnt = 0;
//             num_blocks_++;
//         }
//     }

//     if (entryInBlockCnt > 0) {
//         std::string compressed;
//         if (Option::BLOCK_COMPRESSED)
//             snappy::Compress(block.data(), block.size(), &compressed);
//         else
//             compressed = block;
//         blockseq += compressed;
//         oris_.push_back(ori);
//         cmps_.push_back(cmp);
//         ori += block.size();
//         cmp += compressed.size();
//         block.clear();
//         num_blocks_++;
//     }
//     keys_.push_back(0);
//     offsets_.push_back(offset);
//     oris_.push_back(ori);
//     cmps_.push_back(cmp);
//     save(blockseq);
// }

// uint64_t SSTable::no() const {
//     return id_.no_;
// }

// std::string SSTable::loadBlock(uint64_t pos) const {
//     std::string block;
//     char *buf = new char[cmps_[pos + 1] - cmps_[pos]];
//     std::ifstream *ifs = table_cache_->open(id_);
//     ifs->seekg(indexSpace() + cmps_[pos], std::ios::beg);
//     ifs->read(buf, cmps_[pos + 1] - cmps_[pos]);
//     if (Option::BLOCK_COMPRESSED)
//         snappy::Uncompress(buf, cmps_[pos + 1] - cmps_[pos], &block);
//     else
//         block.assign(buf, cmps_[pos + 1] - cmps_[pos]);
//     delete[] buf;
//     return block;
// }

// std::pair<bool, std::string> SSTable::search(uint64_t key) {
//     uint64_t left = 0;
//     uint64_t right = num_entries_;
//     uint64_t mid = 0;
//     while (left < right) {
//         mid = left + (right - left) / 2;
//         if(keys_[mid] == key){
//             break;
//         }else if(keys_[mid] < key){
//             left = mid;
//         }else{
//             right = mid;
//         }
//     }
//     if(keys_[mid] != key){
//         return std::make_pair(false, "");
//     }
//     // found! load block and return result
//     Location lo = locate(mid);
//     return std::make_pair(true, block_cache_->read(lo));
// }

// void SSTable::save(const std::string &blockSeg) const {
//     std::ofstream ofs(id_.name(), std::ios::binary);
//     ofs.write((char*) &num_entries_, sizeof(uint64_t));
//     for (uint64_t i = 0; i <= num_entries_; ++i) {
//         ofs.write((char*) &keys_[i], sizeof(uint64_t));
//         ofs.write((char*) &offsets_[i], sizeof(uint64_t));
//     }
//     ofs.write((char*) &num_blocks_, sizeof(uint64_t));
//     for (uint64_t i = 0; i <= num_blocks_; ++i) {
//         ofs.write((char*) &oris_[i], sizeof(uint64_t));
//         ofs.write((char*) &cmps_[i], sizeof(uint64_t));
//     }
//     ofs.write(blockSeg.data(), cmps_.back());
//     ofs.close();
// }

// uint64_t SSTable::indexSpace() const {
//     return (num_entries_ * 2 + num_blocks_ * 2 + 6) * sizeof(uint64_t);
// }

// Location SSTable::locate(uint64_t pos) const {
//     uint64_t k = 0;
//     while (offsets_[pos + 1] > oris_[k + 1])
//         ++k;
//     return {this, k, offsets_[pos] - oris_[k], offsets_[pos + 1] - offsets_[pos]};
// }