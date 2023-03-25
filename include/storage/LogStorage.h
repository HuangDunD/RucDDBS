#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <sys/stat.h>

static const std::string LOG_FILE_NAME = "db.log";

class LogStorage
{
private:
    // file name for log file
    std::string log_name_;
    // file stream for log file
    std::fstream log_file_;
public:
    LogStorage(std::string db_name_){

        auto n = db_name_.rfind('.');
        log_name_ = db_name_.substr(0, n) + ".log";

        // open log file stream
        log_file_.open(log_name_, std::ios::binary | std::ios::in | std::ios::app | std::ios::out);
        if (!log_file_.is_open()) {
            // log is not opended, which means the file doesn't exist
            // then we create it
            log_file_.clear();
            // std::ios::in will fail us when the file is not exist 
            log_file_.open(log_name_, std::ios::binary | std::ios::trunc | std::ios::app | std::ios::out);
            log_file_.close();
            // reopen it with original mode
            log_file_.open(log_name_, std::ios::binary | std::ios::in | std::ios::app | std::ios::out);
            if (!log_file_.is_open()) {
                std::cerr << ("failed to open log file, filename: %s", log_name_.c_str());
            }
        }
    };
    ~LogStorage();

    int GetFileSize(const std::string &filename) {
        struct stat stat_buf;
        int rc = stat(filename.c_str(), &stat_buf);
        return rc == 0 ? static_cast<int> (stat_buf.st_size) : -1;
    }

    bool ReadLog(char *log_data, int size, int offset) {

        // this is stupid. we should cache the log size then read it till end
        // since log file won't change while performing recovery protocol.
        if (offset >= GetFileSize(log_name_)) {
            return false;
        }

        log_file_.seekp(offset);
        log_file_.read(log_data, size);

        if (log_file_.bad()) {
            std::cerr << ("I/O error while reading log");
            return false;
        }

        int read_count = log_file_.gcount();
        if (read_count < size) {
            log_file_.clear();
            // pad with zero
            memset(log_data + read_count, 0, size - read_count);
        }

        return true;
    }

    void WriteLog(char *log_data, int size) {
        if (size == 0) {
            return;
        }
        // append the log
        log_file_.write(log_data, size);
        // check for IO-error
        if (log_file_.bad()) {
            std::cerr << "I/O error while writing log";
            return;
        }
        // flush to disk
        log_file_.flush();
    }
};
