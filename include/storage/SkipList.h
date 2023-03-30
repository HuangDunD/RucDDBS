#ifndef STORAGE_SKIPLIST_H
#define STORAGE_SKIPLIST_H

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
    /**
        insert (key, value) pair into the skiplist
        if there already exists a key that equals to the given key,
        the function will update the value
    */
    void put(const std::string &key, const std::string& value);
    std::pair<bool, std::string> get(const std::string &key) const;
    bool del(const std::string &key);
    bool contains(const std::string &key) const;
    Iterator iterator() const;
    size_t size() const;
    bool empty() const;
    void clear();
    // size_t space() const;
 private:
    struct Node;
    const size_t max_height_ = 12;
    Node *head_, *tail_;
    size_t num_entries_;
    size_t num_bytes_;
    std::default_random_engine engine_;
    std::uniform_int_distribution<int> dist_;
    void init();
    // Node *find(const std::string &key) const;
    void enlargeHeight(size_t height);
    size_t random_height();
    size_t getMaxHeight() const;
    Node *findGreatorOrEqual(const std::string &key, Node **prevs) const;
};

struct SkipList::Node {
    std::string key_;
    std::string value_;
    Node **prevs_;
    Node **nexts_;
    size_t height_;
    explicit Node(const std::string &key, const std::string &value, size_t height);
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