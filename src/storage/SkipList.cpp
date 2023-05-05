#include <iostream>
#include <cassert>

#include "SkipList.h"
#include "Option.h"

SkipList::SkipList() : dist_(0, 1)
{
    init();
}

SkipList::~SkipList()
{
    clear();
    delete head_;
    delete tail_;
}

// clear all nodes in skiplist, include head_ and tail_
void SkipList::clear()
{
    for (Node *node = head_; node != nullptr;)
    {
        Node *next = node->nexts_[0];
        delete node;
        node = next;
    }
    init();
}

// 功能：获得给定key所对应的value。
// 返回值：如果跳表中存在对应的键值对，那么返回(true, value)，否则返回(false, "")
std::pair<bool, std::string> SkipList::get(const std::string &key) const
{

}

// 功能：向跳表中插入键值对(key, value)。如果跳表中已经存在与给定key相同key的键值对，那么更新该key所对应的value
// 返回值：void
void SkipList::put(const std::string &key, const std::string &value)
{

}

// 功能：删除跳表中的指定键值对(key, value)。
// 返回值：如果删除成功返回true，否则返回false，如果指定的key不存在，返回false
bool SkipList::del(const std::string &key)
{

}

// 功能：返回跳表是否存在指定的key。
// 返回值：如果指定的key存在，返回true，否则返回false
bool SkipList::contains(const std::string &key) const
{
    
}


void SkipList::init()
{
    head_ = new Node("", "", 1);
    tail_ = new Node("", "", 1);
    head_->nexts_[0] = tail_;
    tail_->nexts_[0] = nullptr;
    num_bytes_ = 0;
    num_entries_ = 0;
}

// 返回一个随机的高度值，小于max_height_
size_t SkipList::random_height()
{
    size_t height = 1;
    while (dist_(engine_))
        ++height;
    return height > max_height_ ? max_height_ : height;
}

size_t SkipList::getMaxHeight() const
{
    return head_->height_;
}

// 功能：当新节点的高度超过当前的最高节点高度（即head和tail节点的高度）时，需要扩充跳表的最高节点（即head和tail节点）的高度
// 返回值：void
void SkipList::enlargeHeight(size_t height)
{

}

// 功能：查找大于或者等于指定key的第一个节点。
// 返回值：如果没有节点大于指定的key，那么返回tail节点。如果prevs不等于nullptr，那么prevs会保存该节点的所有前缀节点。
SkipList::Node *SkipList::findGreatorOrEqual(const std::string &key, Node **prevs) const
{
    
}

// 功能：查找最后一个小于指定key的节点
// 返回值：如果没有节点小于指定的key，那么返回head节点
SkipList::Node* SkipList::findLessThan(const std::string &key) const {
    
}

// 功能：查找除tail结点之外的最后一个节点。
SkipList::Node *SkipList::findLast() const {
    
}

// Node
SkipList::Node::Node(const std::string &key, const std::string &value, size_t height)
    : key_(key), value_(value), height_(height)
{
    nexts_ = new Node *[height];
}

SkipList::Node::~Node()
{
    delete[] nexts_;
}

//Iterator
SkipList::Iterator::Iterator(const SkipList *skiplist) : skiplist_(skiplist), node_(nullptr){

}

// 
bool SkipList::Iterator::Valid() const {
    return node_ != nullptr;
}

// 
void SkipList::Iterator::SeekToFirst() {
    node_ = skiplist_->head_->nexts_[0];
    if(node_ == skiplist_->tail_){
        node_ = nullptr;
    }
}

// 
void SkipList::Iterator::SeekToLast() {
    node_ = skiplist_->findLast(); 
    if(node_ == skiplist_->head_){
        node_ = nullptr;
    }   
}

//
void SkipList::Iterator::Seek(const std::string &key) {
    node_ = skiplist_->findGreatorOrEqual(key, nullptr);
    if(node_ == skiplist_->tail_) {
        node_ = nullptr;
    }
}

//
void SkipList::Iterator::Next() {
    assert(Valid());
    node_ = node_->nexts_[0];
    if(node_ == skiplist_->tail_){
        node_ = nullptr;
    }
}

//
void SkipList::Iterator::Prev() {
    assert(Valid());
    node_ = skiplist_->findLessThan(node_->key_);
    if(node_ == skiplist_->head_){
        node_ = nullptr;
    }
}

//
std::string SkipList::Iterator::key() const {
    assert(Valid());
    return node_->key_;
}

//
std::string SkipList::Iterator::value() const {
    assert(Valid());
    return node_->value_;
}