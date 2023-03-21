#include <string>

#include "SkipList.h"

#include "Option.h"


SkipList::SkipList(): dist_(0, 1) {
    init();
}

SkipList::~SkipList() {
    clear();
    delete head_;
    delete tail_;
}

// clear all nodes in skiplist, include head_ and tail_
void SkipList::clear() {
    for (Node *node = head_; node != nullptr;) {
        Node *next = node->nexts_[0];
        delete node;
        node = next;
    }
    init();
}

// get the value of the key, if not found, return empty string
std::string SkipList::get(uint64_t key) const {
    Node *node = find(key);
    return node != head_ && node->key_ == key ? node->value_ : "";
}

// insert a new (key, value) pair into skiplist, if the key is already in
// the skiplist, then update the value, if not, then create a new node using
void SkipList::put(uint64_t key, const std::string &value) {
    Node *prev = find(key);
    if (prev != head_ && prev->key_ == key) {
        prev->value_ = value;
        return;
    }
    size_t height = random_height();
    // the height of head_ is larger than that of every node
    if (head_->height_ < height + 1)
        enlargeHeight(height + 1);
    Node *node = new Node(key, value, height);
    for (size_t i = 0; i < height; ++i) {
        node->prevs_[i] = prev;
        node->nexts_[i] = prev->nexts_[i];
        prev->nexts_[i]->prevs_[i] = node;
        prev->nexts_[i] = node;
        while (i + 1 >= prev->height_)
            prev = prev->prevs_[i];
    }
    ++num_entries_;
    num_bytes_ += value.size();
}

// delete (key, value) pair
bool SkipList::del(uint64_t key) {
    Node *node = find(key);
    if (node == head_ || node->key_ != key)
        return false;
    size_t height = node->height_;
    for (size_t lvl = 0; lvl < height; ++lvl) {
        node->prevs_[lvl]->nexts_[lvl] = node->nexts_[lvl];
        node->nexts_[lvl]->prevs_[lvl] = node->prevs_[lvl];
    }
    --num_entries_;
    num_bytes_ -= node->value_.size();
    delete node;
    return true;
}

bool SkipList::contains(uint64_t key) const {
    Node *node = find(key);
    return node != head_ && node->key_ == key;
}

SkipList::Iterator SkipList::iterator() const {
    return {head_->nexts_[0]};
}

size_t SkipList::size() const {
    return num_entries_;
}

bool SkipList::empty() const {
    return num_entries_ == 0;
}

uint64_t SkipList::space() const {
    return (num_entries_ * 2 + num_bytes_ / Option::BLOCK_SPACE * 2 + 6) * sizeof(uint64_t) + num_bytes_;
}

// init function, only head_ and tail_ two nodes in skiplist
void SkipList::init() {
    head_ = new Node(0, "", 1);
    tail_ = new Node(UINT64_MAX, "", 1);
    head_->prevs_[0] = nullptr;
    head_->nexts_[0] = tail_;
    tail_->prevs_[0] = head_;
    tail_->nexts_[0] = nullptr;
    num_bytes_ = 0;
    num_entries_ = 0;
}

// TODO: rewrite the function without prevs_
// find the latest node whose key is no larger than the key
SkipList::Node *SkipList::find(uint64_t key) const {
    Node *node = head_;
    size_t height = head_->height_;
    for (size_t i = 1; i <= height; ++i) {
        while (node->key_ <= key)
            node = node->nexts_[height - i];
        node = node->prevs_[height - i];
    }
    return node;
}

size_t SkipList::random_height() {
    size_t height = 1;
    while(dist_(engine_))
        ++height;
    return height;
}

void SkipList::enlargeHeight(size_t height) {
    size_t oldHeight = head_->height_;
    head_->height_ = height;
    tail_->height_ = height;
    Node **oldHeadPrevs = head_->prevs_;
    Node **oldHeadNexts = head_->nexts_;
    Node **oldTailPrevs = tail_->prevs_;
    Node **oldTailNexts = tail_->nexts_;
    head_->prevs_ = new Node*[height];
    head_->nexts_ = new Node*[height];
    tail_->prevs_ = new Node*[height];
    tail_->nexts_ = new Node*[height];
    for (size_t i = 0; i < height; ++i)
        head_->prevs_[i] = tail_->nexts_[i] = nullptr;
    for (size_t i = 0; i < oldHeight; ++i) {
        head_->nexts_[i] = oldHeadNexts[i];
        tail_->prevs_[i] = oldTailPrevs[i];
    }
    for (size_t i = oldHeight; i < height; ++i) {
        head_->nexts_[i] = tail_;
        tail_->prevs_[i] = head_;
    }
    delete[] oldHeadPrevs;
    delete[] oldHeadNexts;
    delete[] oldTailPrevs;
    delete[] oldTailNexts;
}

SkipList::Node::Node(uint64_t key, const std::string &value, size_t height)
    : key_(key), value_(value), height_(height) {
    prevs_ = new Node*[height];
    nexts_ = new Node*[height];
}

SkipList::Node::~Node() {
    delete[] prevs_;
    delete[] nexts_;
}

SkipList::Iterator::Iterator(Node *node): node_(node) {}

Entry SkipList::Iterator::next() {
    Entry entry(node_->key_, node_->value_);
    if (node_->nexts_[0] != nullptr)
        node_ = node_->nexts_[0];
    return entry;
}

bool SkipList::Iterator::hasNext() const {
    return node_->nexts_[0] != nullptr;
}