#pragma once
#include <cstdint>
#include <string>
#include <assert.h>

using lsn_t = int32_t;
using txn_id_t = uint64_t;

static constexpr int INVALID_TXN_ID = 0;                                     // invalid transaction id
static constexpr int INVALID_LSN = -1;                                        // invalid log sequence number

enum class LogRecordType { INVALID = 0, CREATE_TABLE, MARK_DROP_TABLE, APPLY_DROP_TABLE, 
                            CREATE_INDEX, MARK_DROP_INDEX, APPLY_DROP_INDEX,
                            INSERT, UPDATE, DELETE, BEGIN, COMMIT, ABORT};

static std::string log_record_type[15] = {"INVALID", "CREATE_TABLE", "MARK_DROP_TABLE","APPLY_DROP_TABLE",
                                   "CREATE_INDEX", "MARK_DROP_INDEX", "APPLY_DROP_INDEX",
                                   "INSERT", "UPDATE", "DELETE", "BEGIN", "COMMIT", "ABORT"};

class LogRecord {

public:
    LogRecord() = default;

    // constructor for transaction_operation (begin/commit/abort)
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_type)
        : size_(HEADER_SIZE), txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_type) {
            assert(log_type == LogRecordType::BEGIN || log_type == LogRecordType::COMMIT || log_type == LogRecordType::ABORT);
        }

    static constexpr int HEADER_SIZE = 20;


private:
    int32_t size_{0};
    lsn_t lsn_{INVALID_LSN};
    txn_id_t txn_id_{INVALID_TXN_ID};
    lsn_t prev_lsn_{INVALID_LSN};
    LogRecordType log_type_{LogRecordType::INVALID};
};