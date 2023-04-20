#include <cstring>
#include <cassert>

#include <iostream>

#include "Option.h"
#include "Block.h"

Block::Block(const BlockContents &block_content) : data_(block_content.data), size_(block_content.size), own_(block_content.heap_allocated)
{
    // 读取restart_offset和restart_size
    std::memcpy(&restart_offset_, data_ + size_ - sizeof(uint64_t), sizeof(uint64_t));
    std::memcpy(&restart_size_, data_ + restart_offset_, sizeof(uint64_t));
}

Block::~Block() {
    if(own_) {
        delete [] data_;
    }
}

// TODO: 暂时全局顺序查找，后续优化为先使用二分查找restart，然后顺序查找内部
std::pair<bool, std::string> Block::get(const std::string &key) const {
    auto iter = NewIterator();
    iter->Seek(key);
    if(iter->Valid() && iter->Key() == key) {
        return std::make_pair(true, iter->Value());
    }
    return std::make_pair(false, "");
}

// std::unique_ptr<Iterator> Block::NewIterator() const {
std::unique_ptr<Iterator> Block::NewIterator() const {
    // return new Iter(this);
    return std::make_unique<Iter>(this);
}

bool Block::Entry::DecodeFrom(const char *data, uint64_t offset) {
    uint64_t key_size, value_size;
    std::memcpy(&key_size, data + offset, sizeof(uint64_t));
    key_ = std::string(data + offset + sizeof(uint64_t), key_size);
    std::memcpy(&value_size, data + offset + sizeof(uint64_t) + key_size, sizeof(uint64_t));
    value_ = std::string(data + offset + sizeof(uint64_t) * 2 + key_size, value_size);
    return true;
}

Block::Iter::Iter(const Block *block) : data_(block->data_), size_(block->size_),
                                    restart_offset_(block->restart_offset_), restart_size_(block->restart_size_){
    current_ = restart_offset_;
    restart_index_ = restart_size_;
}

// 
void Block::Iter::SeekToFirst() {
    std::scoped_lock lock(mutex_);
    current_ = 0;
    restart_index_ = 0;
    entry_.DecodeFrom(data_, current_);
}

// 
void Block::Iter::SeekToLast() {
    std::scoped_lock lock(mutex_);
    // set restart_index
    restart_index_ = restart_size_ - 1;
    // find the last
    uint64_t offset = getRestartPoint(restart_index_);
    entry_.DecodeFrom(data_, offset);
    while(offset + entry_.size() < restart_offset_) {
        offset += entry_.size();
        entry_.DecodeFrom(data_, offset);
    }
    current_ = offset;
}

// 二分查找
void Block::Iter::Seek(const std::string &key) {
    std::scoped_lock lock(mutex_);
    // 二分查找位于哪一个restart_index中
    int left = 0, right = restart_size_ - 1;
    int mid;
    uint64_t offset;
    Block::Entry mid_entry;
    while(left <= right) {
        mid = left + (right - left) / 2;
        offset = getRestartPoint(mid);
        mid_entry.DecodeFrom(data_, offset);
        if(mid_entry.key() >= key) {
            right = mid - 1;
        }else if(mid_entry.key() < key) {
            left = mid + 1;
        }else{
            break;
        }
    }
    // 如果right小于0，那么说明没有找到，定位到First
    if(right < 0){
        restart_index_ = 0;
        current_ = 0;
        entry_.DecodeFrom(data_, current_);
        return ;
    }
    restart_index_ = right;
    current_ = getRestartPoint(restart_index_);
    while(current_ < restart_offset_) {
        entry_.DecodeFrom(data_, current_);
        if(entry_.key() >= key) {
            return ;
        }
        current_ += entry_.size();
    }
    current_ = restart_offset_;
    // for(uint64_t i = 0; i < Option::RESTART_INTERVAL; i++) {
    //     entry_.DecodeFrom(data_, current_);
    //     // std::cout << __FILE__ << __LINE__ << ", current key = " << entry_.key() << std::endl;
    //     if(entry_.key() >= key) {
    //         return ;
    //     }
    //     if(current_ + entry_.size() >= restart_offset_) {
    //         current_ = restart_offset_;
    //         return ;
    //     }else {
    //         current_ += entry_.size();
    //     }
    // }
}
    
//
void Block::Iter::Next() {
    std::scoped_lock lock(mutex_);
    if(current_ + entry_.size() >= restart_offset_) {
        current_ = restart_offset_;
        return ;
    }
    current_ += entry_.size();
    entry_.DecodeFrom(data_, current_);

    if(restart_index_ + 1 < restart_size_){
        uint64_t offset = getRestartPoint(restart_index_ + 1);
        if(offset == current_) {
            restart_index_ = restart_index_ + 1;
        }
    }
}
    
//
void Block::Iter::Prev() {
    std::scoped_lock lock(mutex_);
    if(current_ == 0) {
        current_ = restart_offset_;
        return ;
    }
    uint64_t offset = getRestartPoint(restart_index_);
    if(offset == current_) {
        restart_index_ = restart_index_ - 1;
        offset = getRestartPoint(restart_index_);
    }
    entry_.DecodeFrom(data_, offset);
    while(offset + entry_.size() < current_){
        offset += entry_.size();
        entry_.DecodeFrom(data_, offset);
    }
    current_ = offset;
}

