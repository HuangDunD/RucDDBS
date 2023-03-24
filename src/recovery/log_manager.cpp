#include "log_manager.h"
#include <iostream>

lsn_t LogManager::AppendLogRecord(LogRecord *log_record) {
    std::unique_lock<std::mutex> l(latch_);
    if (log_buffer_write_offset_ + log_record->GetSize() >= LOG_BUFFER_SIZE) {
        cv_.notify_one(); //let RunFlushThread wake up.
        operation_cv_.wait(l, [&] {return log_buffer_write_offset_ + log_record->GetSize()< LOG_BUFFER_SIZE;});
    }
    memcpy(log_buffer_ + log_buffer_write_offset_, &log_record, LogRecord::HEADER_SIZE);
    int pos = log_buffer_write_offset_ + LogRecord::HEADER_SIZE;

    if(log_record->GetLogRecordType() == LogRecordType::INSERT ||
            log_record->GetLogRecordType() == LogRecordType::DELETE ){
        memcpy(log_buffer_ + pos, &log_record->GetKey(), sizeof(log_record->GetKey()));
        pos += sizeof(log_record->GetKey());
        memcpy(log_buffer_ + pos, &log_record->GetValue(), sizeof(log_record->GetValue()));
        pos += sizeof(log_record->GetValue());
    }
    else if(log_record->GetLogRecordType() == LogRecordType::UPDATE){
        memcpy(log_buffer_ + pos, &log_record->GetKey(), sizeof(log_record->GetKey()));
        pos += sizeof(log_record->GetKey());
        memcpy(log_buffer_ + pos, &log_record->GetValue(), sizeof(log_record->GetValue()));
        pos += sizeof(log_record->GetValue());
        memcpy(log_buffer_ + pos, &log_record->GetOldValue(), sizeof(log_record->GetOldValue()));
        pos += sizeof(log_record->GetOldValue());
    }
    log_buffer_write_offset_ = pos;
    return next_lsn_++;
};

void LogManager::RunFlushThread(){
    enable_flushing_.store(true);
    flush_thread_ = new std::thread([&]{
        while (enable_flushing_) {
            std::unique_lock<std::mutex> l(latch_);
            //每隔LOG_TIMEOUT刷新一次或者buffer已满或者强制刷盘
            cv_.wait_for(l, LOG_TIMEOUT);
            // swap buffer
            SwapBuffer();

            lsn_t lsn = next_lsn_ - 1;
            uint32_t flush_size = flush_buffer_write_offset_;
            l.unlock();
            // resume the append log record operation 
            operation_cv_.notify_all();
            
            //flush log
            disk_manager_->WriteLog(flush_buffer_, flush_size);
            persistent_lsn_.store(lsn);
        }
    });
}

void LogManager::SwapBuffer() {
    // we are in the critical section, it's safe to exchange these two variable
    std::swap(log_buffer_, flush_buffer_);
    flush_buffer_write_offset_ =  log_buffer_write_offset_ ;
    log_buffer_write_offset_ = 0;
}

void LogManager::Flush(lsn_t lsn, bool force) {
    if (force) {
        // notify flush thread to start flushing the log
        cv_.notify_one();
    }
    while (persistent_lsn_.load() < lsn) {}
}


int main(){
    std::cout << "test";
}