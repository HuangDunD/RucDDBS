#pragma once
#include <cstdint>
#include <string>
#include <assert.h>

using lsn_t = int32_t;
using txn_id_t = uint64_t;

static constexpr int INVALID_TXN_ID = 0;                                     // invalid transaction id
static constexpr int INVALID_LSN = -1;                                        // invalid log sequence number

enum class LogRecordType { INVALID = 0, CREATE_TABLE, DROP_TABLE, INSERT, 
                                UPDATE, DELETE, BEGIN, COMMIT, ABORT, PREPARED};

static std::string log_record_type[15] = {"INVALID", "CREATE_TABLE", "DROP_TABLE", "INSERT", 
                                            "UPDATE", "DELETE", "BEGIN", "COMMIT", "ABORT", "PREPARED"};

/*
 * For EACH log record, HEADER is like (5 fields in common, 24 bytes in total).
 *------------------------------------------------------
 * | size | LSN | transID(8 bytes) | prevLSN | LogType |
 *------------------------------------------------------
 
 */
class LogRecord {

public:
    LogRecord() = default;

    static constexpr int HEADER_SIZE = 24;

    // constructor for transaction_operation (begin/commit/abort/prepared)
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_type)
        : size_(HEADER_SIZE), txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_type) {
            assert(log_type == LogRecordType::BEGIN || log_type == LogRecordType::COMMIT || log_type == LogRecordType::ABORT || log_type == LogRecordType::PREPARED);
        }

    // constructor for INSERT/DELETE type
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_record_type, 
                    uint32_t key_size, const char* key, uint32_t value_size, const char* value)
        : txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_record_type), key_size_(key_size), key_(key), value_size_(value_size), value_(value) {
        // calculate log record size
        size_ = HEADER_SIZE + sizeof(int32_t) + key_size_ + sizeof(int32_t) + value_size_;
    }

    // constructor for UPDATE type
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_record_type, 
                    uint32_t key_size, const char* key, uint32_t value_size, const char* value, uint32_t old_value_size, const char* old_value)
        : txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_record_type), key_size_(key_size), key_(key), 
                value_size_(value_size), value_(value), old_value_size_(old_value_size), old_value_(old_value){
        // calculate log record size
        size_ = HEADER_SIZE + sizeof(int32_t) + key_size_ + sizeof(int32_t) + value_size_ + sizeof(int32_t) + old_value_size_;
    }
    
    inline lsn_t GetLsn() { return lsn_; }
    inline void SetLsn(lsn_t lsn) { lsn_ = lsn; }
    inline lsn_t GetPrevLsn() { return prev_lsn_; }
    inline txn_id_t GetTxnId() { return txn_id_; }
    inline int32_t GetSize() { return size_; }
    inline LogRecordType GetLogRecordType() { return log_type_; }
    
    inline const uint32_t &GetKeySize() {return key_size_;}
    inline const uint32_t &GetValueSize() {return value_size_;}
    inline const uint32_t &GetOldValueSize() {return old_value_size_;}
    inline const char* GetKey()  { return key_; }
    inline const char* GetValue()  { return value_; }
    inline const char* GetOldValue()  { return old_value_; }

private:
    int32_t size_{0};
    lsn_t lsn_{INVALID_LSN};
    txn_id_t txn_id_{INVALID_TXN_ID};
    lsn_t prev_lsn_{INVALID_LSN};
    LogRecordType log_type_{LogRecordType::INVALID};

    //use for insert and delete
    uint32_t key_size_;
    const char* key_;
    uint32_t value_size_;
    const char* value_;

    //use for update 
    uint32_t old_value_size_;
    const char* old_value_;

};