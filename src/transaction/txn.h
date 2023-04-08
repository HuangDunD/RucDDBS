#pragma once

#include <cstdint>
#include <cassert>
#include <unordered_map>
#include "string.h"

using table_oid_t = int32_t;
using row_id_t = int32_t;
using partition_id_t = int32_t;

// use for write set and transaction abort
enum class WType { INSERT_TUPLE = 0, DELETE_TUPLE, UPDATE_TUPLE};
class WriteRecord {
   public:
    WriteRecord() = default;
    // for insert record
    WriteRecord(char* new_key, int32_t key_size, WType type){
        assert(type == WType::INSERT_TUPLE);
        wtype_ = type;
        key_size_ = key_size;
        
        key_ = new char[key_size_];
        memcpy(key_, new_key, key_size_);
    }

    // for delete record, need record delete_value for abort
    // because we may flush memtable into disk and don't know the delete_value
    WriteRecord(char* delete_key, int32_t key_size, char* delete_value, int32_t value_size, WType type){
        assert(type == WType::DELETE_TUPLE);
        wtype_ = type;
        key_size_ = key_size;
        value_size_ = value_size;

        key_ = new char[key_size_];
        memcpy(key_, delete_key, key_size_);
        value_ = new char[value_size_];
        memcpy(value_, delete_value, value_size_);        
        
    }

    // for update record
    WriteRecord(char* update_key, int32_t key_size, char* new_value, int32_t value_size,
            char* old_value, int32_t old_value_size, WType type){
        
        wtype_ = type;
        key_size_ = key_size;
        value_size_ = value_size;
        old_value_size_ = old_value_size;

        key_ = new char[key_size];
        memcpy(key_, update_key, key_size);
        value_ = new char[value_size];
        memcpy(value_, new_value, value_size);
        old_value_ = new char[old_value_size];
        memcpy(old_value_, old_value, old_value_size);
        
    }
    ~WriteRecord(){
        delete[] key_;
        delete[] value_;
        delete[] old_value_;
        key_ = nullptr;
        value_ = nullptr;
        old_value_ = nullptr;
    }
    WType getWType() const{return wtype_;}
    int32_t getKeySize() const {return key_size_;}
    char* getKey() const {return key_;}
    int32_t getValueSize() const {return value_size_;}
    char* getValue() const {return value_;}
    int32_t getOldValueSize() const {return old_value_size_;}
    char* getOldValue() const {return old_value_;}
   private:
    WType wtype_;
    char* key_;
    int32_t key_size_;

    char* value_;
    int32_t value_size_;

    char* old_value_;
    int32_t old_value_size_;
};

enum class Lock_data_type {TABLE, PARTITION, ROW};
enum class LockMode { SHARED, EXLUCSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE, S_IX };

class Lock_data_id
{
public:
    table_oid_t oid_ = 0;
    partition_id_t p_id_ = 0;
    row_id_t row_id_ = 0;
    Lock_data_type type_;

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