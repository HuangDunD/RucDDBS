#pragma once
#include "log_record.h"

class LogManager
{
private:
    /* data */
public:
    lsn_t AppendLogRecord(LogRecord *log_record) {
        //TODO
        return INVALID_LSN;
    };
    LogManager(){};
    ~LogManager(){};
};
