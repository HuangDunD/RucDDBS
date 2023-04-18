#include <cstring>

#include "format.h"


BlockHandle::BlockHandle(uint64_t offset, uint64_t size) : offset_(offset), size_(size) {
    
}

void BlockHandle::EncodeInto(std::string &s) {
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


