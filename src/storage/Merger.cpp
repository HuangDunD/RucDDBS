#include <iostream>

#include "Merger.h"


std::unique_ptr<Iterator> NewMergingIterator(std::vector<std::unique_ptr<Iterator>> children) {
    if(children.size() == 0) {
        return nullptr;
    }else if(children.size() == 1) {
        return std::move(children[0]);
    }else {
        return std::make_unique<MergerIterator>(std::move(children));
    }
    
}


// 初始化
MergerIterator::MergerIterator(std::vector<std::unique_ptr<Iterator>> children) 
                                        : children_(std::move(children)), current_(children_.size()), direction_(kForward)
{
    // std::cout << __FILE__ << __LINE__ << "children.size = " << children_.size() << std::endl;
}

MergerIterator::~MergerIterator() {

}

void MergerIterator::SeekToFirst() {
    std::scoped_lock lock(latch_);
    for(size_t i = 0; i < children_.size(); i++) {
        children_[i]->SeekToFirst();
    }
    // std::cout << __FILE__ << __LINE__ << std::endl;
    find_smallest();
    direction_ = kForward;
}

void MergerIterator::Seek(const std::string &key) {
    std::scoped_lock lock(latch_);
    for(size_t i = 0; i < children_.size(); i++) {
        children_[i]->Seek(key);
    }
    find_smallest();
    direction_ = kForward;
}

void MergerIterator::SeekToLast() {
    std::scoped_lock lock(latch_);
    for(size_t i = 0; i < children_.size(); i++) {
        children_[i]->SeekToLast();
    }
    find_largest();
    direction_ = kReverse;
}

void MergerIterator::Next() {
    assert(Valid());
    std::scoped_lock lock(latch_);
    if(direction_ != kForward) {
        for(size_t i = 0; i < children_.size(); i++) {
            if(i != current_) {
                children_[i]->Seek(Key());
                if(children_[i]->Valid() && children_[i]->Key() == Key()) {
                    children_[i]->Next();
                }
            }
        }
    }

    children_[current_]->Next();
    find_smallest();
}

void MergerIterator::Prev() {
    assert(Valid());
    std::scoped_lock lock(latch_);
    if(direction_ != kReverse) {
        for(size_t i = 0; i < children_.size(); i++) {
            if(i != current_) {
                children_[i]->Seek(Key());
            }
            if(children_[i]->Valid()) {
                children_[i]->Prev();
            }else {
                children_[i]->SeekToLast();
            }
        }
    }
    children_[current_]->Prev();
    find_largest();
}

// 默认反向扫描
void MergerIterator::find_smallest() {
    size_t smallest = children_.size();
    for(int i = children_.size() - 1; i >= 0; i--) {
        if(children_[i]->Valid()) {
            if(smallest >= children_.size()) {
                smallest = i;
            }else if(children_[i]->Key() < children_[smallest]->Key()){
                smallest = i;
            }
        }
    }
    // std::cout << __FILE__ << __LINE__ << "Found smallest = " << smallest << std::endl;
    current_ = smallest;
}

// 默认反向扫描
void MergerIterator::find_largest() {
    size_t largest = children_.size();
    for(int i = children_.size() - 1; i >= 0; i--) {
        if(children_[i]->Valid()) {
            if(largest >= children_.size()) {
                largest = i;
            }else if(children_[i]->Key() > children_[largest]->Key()) {
                largest = i;
            }
        }
    }
    
    current_ = largest;
}