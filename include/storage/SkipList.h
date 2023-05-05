#ifndef STORAGE_SKIPLIST_H
#define STORAGE_SKIPLIST_H

#include <string>
#include <cstddef>
#include <random>
#include <utility>

class SkipList
{
public:
    class Iterator;
    explicit SkipList();
    ~SkipList();

    // 向跳表中插入键值对(key, value)。如果跳表中已经存在与给定key相同key的键值对，那么更新该key所对应的value
    void put(const std::string &key, const std::string &value);

    // 获得给定key所对应的value。如果跳表中存在对应的键值对，那么返回(true, value)，否则返回(false, "")
    std::pair<bool, std::string> get(const std::string &key) const;

    // 删除跳表中的指定键值对(key, value)。如果删除成功返回true，否则返回false，如果指定的key不存在，返回false
    bool del(const std::string &key);

    // 返回跳表是否存在指定的key。如果指定的key存在，返回true，否则返回false
    bool contains(const std::string &key) const;

    // Iterator newIterator() const;

    // return how many (key, value) pairs in skiplist
    inline size_t size() const { return num_entries_; }

    // return the space of skiplist
    inline size_t space() const { return num_bytes_; }

    // return true, if the skiplist is empty
    inline bool empty() const { return num_entries_ ==  0; }

    // clear the skiplist
    // Require: before clear the skiplist, store the data in SSTable
    void clear();

    // return the amount of skiplist's space
    // size_t space() const;
private:
    struct Node;

    const size_t max_height_ = 12;

    Node *head_, *tail_;

    size_t num_entries_;

    size_t num_bytes_;

    std::default_random_engine engine_;
    std::uniform_int_distribution<int> dist_;

    // create head_ and tail_ node in skiplist
    void init();

    // 当新节点的高度超过当前的最高节点高度（即head和tail节点的高度）时，需要扩充跳表的最高节点（即head和tail节点）的高度
    void enlargeHeight(size_t height);

    // return a random height that is not greater than max_height_
    size_t random_height();

    // return the current max height in skiplist
    size_t getMaxHeight() const;

    // 查找大于或者等于指定key的第一个节点，如果没有节点大于指定的key，那么返回tail节点。如果prevs不等于nullptr，那么prevs会保存该节点的所有前缀节点。
    Node *findGreatorOrEqual(const std::string &key, Node **prevs) const;
    
    // 查找最后一个小于指定key的节点，如果没有节点小于指定的key，那么返回head节点
    Node *findLessThan(const std::string &key) const;

    // 查找除`tail`结点之外的最后一个节点。
    Node *findLast() const;
};

struct SkipList::Node
{
    std::string key_;
    std::string value_;
    Node **nexts_;
    size_t height_;
    explicit Node(const std::string &key, const std::string &value, size_t height);
    Node() = delete;
    ~Node();
};

class SkipList::Iterator {
public:
    explicit Iterator(const SkipList *skiplist);

    // don't use copy constructor and assignment operator
    Iterator(const Iterator &) = delete;
    Iterator &operator=(const Iterator &) = delete;

    ~Iterator() = default;

    // Is iterator at a key/value pair. Before use, call this function
    bool Valid() const;

    // 
    void SeekToFirst();

    // 
    void SeekToLast();

    //
    void Seek(const std::string &key);
    
    //
    void Next();
    
    //
    void Prev();

    //
    std::string key() const;

    //
    std::string value() const;
private:
    const SkipList *skiplist_;
    Node *node_;
};

#endif