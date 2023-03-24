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
 *---------------------------------------------
 * | size | LSN | transID | prevLSN | LogType |
 *---------------------------------------------
 
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
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_record_type, const std::string &key, const std::string &value)
        : txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_record_type), key_(key), value_(value) {
        // calculate log record size
        size_ = HEADER_SIZE + sizeof(key) /*+ sizeof(int32_t) */ + sizeof(value);
    }

    // constructor for UPDATE type
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_record_type, const std::string &key, const std::string &value, 
                     const std::string &old_value)
        : txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_record_type), key_(key), value_(value), old_value_(old_value){
        // calculate log record size
        size_ = HEADER_SIZE + sizeof(key) /*+ sizeof(int32_t) */ + sizeof(value) + sizeof(old_value);
    }
    
    inline lsn_t GetLsn() { return lsn_; }
    inline void SetLsn(lsn_t lsn) { lsn_ = lsn; }
    inline lsn_t GetPrevLsn() { return prev_lsn_; }
    inline txn_id_t GetTxnId() { return txn_id_; }
    inline int32_t GetSize() { return size_; }
    inline LogRecordType GetLogRecordType() { return log_type_; }
    
    inline std::string &GetKey()  { return key_; }
    inline std::string &GetValue()  { return value_; }
    inline std::string &GetOldValue()  { return old_value_; }

private:
    int32_t size_{0};
    lsn_t lsn_{INVALID_LSN};
    txn_id_t txn_id_{INVALID_TXN_ID};
    lsn_t prev_lsn_{INVALID_LSN};
    LogRecordType log_type_{LogRecordType::INVALID};

    std::string key_;
    std::string value_;

    std::string old_value_;

};