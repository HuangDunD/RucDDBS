#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cstring>
#include <cassert>

#include "Iterator.h"
#include "format.h"
#include "Option.h"

class Block {
public:
    explicit Block(const BlockContents &block_content);
    // explicit Block(const std::string &block_content);

    // 禁用复制构造函数
    Block(const Block &) = delete;
    Block& operator=(const Block &) = delete;

    ~Block();
    // get(key)
    std::pair<bool, std::string> get(const std::string &key) const;

    // new iterator
    std::unique_ptr<Iterator> NewIterator() const;

    // size of block
    inline size_t size() const { return size_; }

private:
    // 迭代器Iter
    class Entry;
    class Iter;

    // data_, size_, own_
    const char *data_;
    size_t size_;
    bool own_;
    // restart offset and restart_size_
    uint64_t restart_offset_;
    size_t restart_size_;
    // Iterator
    // std::unique_ptr<Iterator> iiter;
};

class Block::Entry {
public:
    Entry() = default;

    bool DecodeFrom(const char *s, uint64_t offset);

    inline const std::string& key() const { return key_; }
    inline const std::string& value() const { return value_; }
    inline uint64_t size() const { return key_.size() + value_.size() + sizeof(uint64_t) * 2; }
private:
    std::string key_;
    std::string value_;
};

class Block::Iter : public Iterator {
public:
    explicit Iter(const Block *block);

    // Is iterator at a key/value pair. Before use, call this function
    inline bool Valid() const override {
        return current_ != restart_offset_;
    }

    // 
    void SeekToFirst() override ;

    // 
    void SeekToLast() override ;

    // 二分查找
    void Seek(const std::string &key) override ;
    
    //
    void Next() override ;
    
    //
    void Prev() override ;

    //
    inline std::string Key() const {
        assert(Valid());
        return entry_.key();
    }

    //
    inline std::string Value() const {
        assert(Valid());
        return entry_.value();
    } 
private:
    std::mutex mutex_;
    // current_目前的offset，restart_index_目前处于的restart_位置。
    uint64_t current_;
    uint64_t restart_index_;
    Block::Entry entry_;
    // 原Block元数据
    const char * const data_;
    const size_t size_;
    const uint64_t restart_offset_;
    const size_t restart_size_;

    // 
    inline uint64_t getRestartPoint(uint64_t index) const{
        assert(index < restart_size_);
        uint64_t offset;
        memcpy(&offset, data_ + restart_offset_ + sizeof(uint64_t) * index + sizeof(uint64_t), sizeof(uint64_t));
        return offset;
    }
};