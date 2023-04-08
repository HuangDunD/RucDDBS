#pragma once

#include <string>
#include <cstdint>

struct TableId{
    const std::string dir_;
    const uint64_t no_;

    explicit TableId(const std::string &dir, const uint64_t no);
    TableId(const TableId &) = default;
    TableId& operator=(const TableId &) = default;
};