#pragma once

#include <string>
#include <sstream>
#include <fstream>

// Block的句柄
class BlockHandle {
public:
    BlockHandle() = default;
    explicit BlockHandle(uint64_t offset, uint64_t size);

    void EncodeInto(std::string &s);
    bool DecodeFrom(const std::string &s);

    inline uint64_t size() { return size_; }
    inline uint64_t offset() { return offset_; }
private:
    uint64_t offset_;
    uint64_t size_;
};

// SSTable的Footer，拥有IndexBlock的句柄和MetaIndexBlock的句柄
class Footer {
public:
    Footer() = default;

    void EncodeInto(std::string &s);
    bool DecodeFrom(const std::string &s);

    inline bool set_meta_index_handle(const BlockHandle meta_index_handle) { meta_index_handle_ = meta_index_handle; return true;}
    inline bool set_index_hanlde(const BlockHandle index_handle) { index_handle_ = index_handle; return true;}

    inline const BlockHandle& meta_index_handle() { return meta_index_handle_; }
    inline const BlockHandle& index_handle() {  return index_handle_; }

private:
    BlockHandle meta_index_handle_;
    BlockHandle index_handle_;
};

// BlockContents读取的内容放在此处，
struct BlockContents {
  char* data;           // Actual contents of data
  uint64_t size;
  bool cachable;        // True iff data can be cached
  bool heap_allocated;  // True iff caller should delete[] data.data()
};

// TODO ReadBlock
// 读取Block至result中，如果成功返回true，否则返回false
bool ReadBlock(std::ifstream *ifs, const BlockHandle& handle, BlockContents* result);



