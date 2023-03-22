#ifndef STORAGE_SKIPLIST_H
#define STORAGE_SKIPLIST_H

#include <cstdint>
#include <string>
#include <cstddef>
#include <random>
#include <utility>

#include "Entry.h"

class SkipList {
 public:
    class Iterator;
    explicit SkipList();
    ~SkipList();
    void put(uint64_t key, const std::string& value);
    std::string get(uint64_t key) const;
    bool del(uint64_t key);
    bool contains(uint64_t key) const;
    Iterator iterator() const;
    size_t size() const;
    bool empty() const;
    void clear();
    uint64_t space() const;
 private:
    struct Node;
    Node *head_, *tail_;
    size_t num_entries_;
    size_t num_bytes_;
    std::default_random_engine engine_;
    std::uniform_int_distribution<int> dist_;
    void init();
    Node *find(uint64_t key) const;
    void enlargeHeight(size_t height);
    size_t random_height();
};

struct SkipList::Node {
    uint64_t key_;
    std::string value_;
    Node **prevs_;
    Node **nexts_;
    size_t height_;
    explicit Node(uint64_t key, const std::string & value, size_t height);
    Node() = delete;
    ~Node();
};

class SkipList::Iterator {
public:
    Entry next();
    bool hasNext() const;
private:
    Node *node_;
    Iterator(Node *node);
friend SkipList;
};

#endif