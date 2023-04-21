#include <sstream>
#include <cassert>
#include <cstring>

#include "SSTableId.h"

SSTableId::SSTableId(const std::string &dir, uint64_t no)
    : dir_(dir), no_(no) {}

SSTableId::SSTableId(const SSTableId &sst) : dir_(sst.dir_), no_(sst.no_) {}

std::string SSTableId::name() const {
    std::ostringstream oss;
    oss << dir_ << "/" << no_ << ".sst";
    return oss.str();
}

void SSTableId::EncodeInto(std::string &s) const {
    uint64_t size = dir_.size();
    s.append((char*)&size, sizeof(uint64_t));
    s.append(dir_, dir_.size());
    s.append((char*)&no_, sizeof(uint64_t));
}
void SSTableId::DecodeFrom(const std::string &s) {
    // assert(s.size > sizeof(uint64_t));
    uint64_t size;
    uint64_t offset = 0;

    memcpy(&size, s.data() + offset, sizeof(uint64_t)); offset += sizeof(uint64_t);
    char buf[size];
    memcpy(buf, s.data() + offset, size); offset += size;
    dir_ = std::string(buf, size);

    memcpy(&no_, s.data() + offset, sizeof(uint64_t));
}