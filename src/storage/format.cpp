#include <cstring>
#include <cassert>

#include "Option.h"
#include "format.h"
#include "snappy.h"

BlockHandle::BlockHandle(uint64_t offset, uint64_t size) : offset_(offset), size_(size) {
    
}

void BlockHandle::EncodeInto(std::string &s) const{
    s.append((char*)&offset_, sizeof(uint64_t));
    s.append((char*)&size_, sizeof(uint64_t));
}

bool BlockHandle::DecodeFrom(const std::string &s) {
    if (s.size() != 2 * sizeof(uint64_t)) {
        return false;
    }
    
    memcpy(&offset_, s.data(), sizeof(uint64_t));
    memcpy(&size_, s.data() + sizeof(uint64_t), sizeof(uint64_t));

    return true;
}

void Footer::EncodeInto(std::string &s) {
    meta_index_handle_.EncodeInto(s);
    index_handle_.EncodeInto(s);
}

bool Footer::DecodeFrom(const std::string &s) {
    return meta_index_handle_.DecodeFrom(s.substr(0, sizeof(uint64_t) * 2)) && index_handle_.DecodeFrom(s.substr(sizeof(uint64_t) * 2, sizeof(uint64_t) * 4));
}

// 
bool ReadBlock(std::ifstream *ifs, const BlockHandle& handle, BlockContents* result) {
    assert(ifs->is_open());

    result->data = nullptr;
    result->cachable = false;
    result->heap_allocated = false;
    
    // 读入
    size_t compressed_length = handle.size();  
    char buf[compressed_length];
    ifs->seekg(handle.offset(), std::ios::beg);
    ifs->read(buf, compressed_length);
    // 解压缩
    char *uncompressed_data;
    size_t uncompressed_length;

    if(Option::BLOCK_COMPRESSED) {
        snappy::GetUncompressedLength(buf, compressed_length, &uncompressed_length);
        uncompressed_data = new char[uncompressed_length];
        snappy::RawUncompress(buf, compressed_length, uncompressed_data);
    }else {
        uncompressed_length = compressed_length;
        char *uncompressed_data = new char[uncompressed_length];
        memcpy(uncompressed_data, buf, uncompressed_length);
    }
    
    // return result
    result->data = uncompressed_data;
    result->size = uncompressed_length;
    result->cachable = true;
    result->heap_allocated = true;

    return true;
}
TableMeta::TableMeta(const SSTableId table_id, const uint64_t num_entries, 
            const uint64_t size, const std::string &first_key, const std::string &last_key)
            : table_id_(table_id), num_entries_(num_entries), size_(size), first_key_(first_key),
            last_key_(last_key)
{

}

void TableMeta::EncodeInto(std::string &s) const {
    uint64_t size = table_id_.name().size();
    s.append((char*)&size, sizeof(uint64_t));
    s.append(table_id_.name().data(), size);

    s.append((char*)&num_entries_, sizeof(uint64_t));
    s.append((char*)&size_, sizeof(uint64_t));

    size = first_key_.size();
    s.append((char*)&size, sizeof(uint64_t));
    s.append(first_key_.data(), first_key_.size());

    size = last_key_.size();
    s.append((char*)&size, sizeof(uint64_t));
    s.append(last_key_.data(), last_key_.size());
}

void TableMeta::DecodeFrom(const std::string &s) {
    uint64_t size;
    uint64_t offset = 0;

    memcpy(&size, s.data() + offset, sizeof(uint64_t)); offset += sizeof(uint64_t);
    char buf[size];
    memcpy(buf, s.data() + offset, size); offset += size;
    table_id_.DecodeFrom(std::string(buf, size));

    memcpy(&num_entries_, s.data() + offset, sizeof(uint64_t)); offset += sizeof(uint64_t);
    memcpy(&size_, s.data() + offset, sizeof(uint64_t)); offset += sizeof(uint64_t);

    memcpy(&size, s.data() + offset, sizeof(uint64_t)); offset += sizeof(uint64_t);
    char fkey[size];
    memcpy(fkey, s.data() + offset, size); offset += size;
    first_key_ = std::string(fkey, size);

    memcpy(&size, s.data() + offset, sizeof(uint64_t)); offset += sizeof(uint64_t);
    char lkey[size];
    memcpy(lkey, s.data() + offset, size); offset += size;
    last_key_ = std::string(lkey, size);
}



