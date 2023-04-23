#pragma once

#include <vector>
#include <memory>
#include <mutex>

#include "SSTable.h"
#include "Iterator.h"

std::unique_ptr<Iterator> NewMergingIterator(std::vector<std::unique_ptr<Iterator>> children);

class MergerIterator : public Iterator {
public:
    MergerIterator(std::vector<std::unique_ptr<Iterator>> children);

    ~MergerIterator();

    inline bool Valid() const override {  return current_ < children_.size(); }

    void SeekToFirst() override;

    void SeekToLast() override;

    void Seek(const std::string &key) override;

    void Next() override;

    void Prev() override;

    inline std::string Key() const override {
        assert(Valid());
        return children_[current_]->Key();
    }

    inline std::string Value() const override {
        assert(Valid());
        return children_[current_]->Value();
    }

private:
    enum Direction { kForward, kReverse };

    const std::vector<std::unique_ptr<Iterator>> children_;
    size_t current_;
    Direction direction_;

    std::mutex latch_;

    void find_smallest();
    void find_largest();
};
