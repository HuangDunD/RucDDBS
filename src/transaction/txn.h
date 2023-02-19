#pragma once

#include <cstdint>
#include <cassert>
#include <unordered_map>

using table_oid_t = int32_t;
using row_id_t = int32_t;
using partition_id_t = int32_t;

enum class Lock_data_type {TABLE, PARTITION, ROW};
enum class LockMode { SHARED, EXLUCSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE, S_IX };

class Lock_data_id
{
public:
    Lock_data_type type_;
    table_oid_t oid_;
    partition_id_t p_id_;
    row_id_t row_id_;

    bool operator==(const Lock_data_id &other) const{
        if(type_ != other.type_) return false;
        if(oid_ != other.oid_) return false;
        if(p_id_ != other.p_id_) return false;
        if(row_id_ != other.row_id_) return false;
        return true;
    }

    // 构造64位大小数据，用于hash，可减少冲突的可能
    // bit structure
    // |    **   |    **********    |  ****************  |   *******************************   |
    // |2bit type|   14bit table    |   16bit partition  |               32 bit rowid          |
    std::size_t Get() const{
        return static_cast<std::size_t> (type_) << 62 | static_cast<std::size_t> (oid_) << 47 | 
            static_cast<std::size_t> (p_id_) << 32 | static_cast<std::size_t> (row_id_);
    }

public:
    Lock_data_id(table_oid_t oid, Lock_data_type type):
        oid_(oid), type_(type){
        assert(type_ == Lock_data_type::TABLE);
    };
    Lock_data_id(table_oid_t oid, partition_id_t p_id, Lock_data_type type):
        oid_(oid), p_id_(p_id), type_(type){
        assert(type_ == Lock_data_type::PARTITION);
    };
    Lock_data_id(table_oid_t oid, partition_id_t p_id, row_id_t row_id, Lock_data_type type):
        oid_(oid), p_id_(p_id), row_id_(row_id), type_(type){
        assert(type_ == Lock_data_type::ROW);
    };
    ~Lock_data_id(){};
};

template<>
struct std::hash<Lock_data_id>
{
    size_t operator() (const Lock_data_id& lock_data) const noexcept
    {
        return hash<size_t>() (lock_data.Get());
    }
}; // 间接调用原生Hash.