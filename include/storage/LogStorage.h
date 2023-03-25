#include <string>
// log file
static const std::string LOG_FILE_NAME = "db.log";

class LogStorage
{
private:
    int log_fd_ = -1;              // log file

public:
    LogStorage(/* args */);
    ~LogStorage();

    bool ReadLog(char *log_data, int size, int offset, int prev_log_end) {
        // read log file from the previous end
        if (log_fd_ == -1) {
            log_fd_ = open_file(LOG_FILE_NAME);
        }
        offset += prev_log_end;
        int file_size = GetFileSize(LOG_FILE_NAME);
        if (offset >= file_size) {
            return false;
        }

        size = std::min(size, file_size - offset);
        lseek(log_fd_, offset, SEEK_SET);
        ssize_t bytes_read = read(log_fd_, log_data, size);
        if (bytes_read != size) {
            throw UnixError();
        }
        return true;
    }

    void WriteLog(char *log_data, int size) {
        if (log_fd_ == -1) {
            log_fd_ = open_file(LOG_FILE_NAME);
        }

        // write from the file_end
        lseek(log_fd_, 0, SEEK_END);
        ssize_t bytes_write = write(log_fd_, log_data, size);
        if (bytes_write != size) {
            throw UnixError();
        }
    }
};
