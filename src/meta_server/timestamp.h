#pragma once
#include <chrono>
#include <string>
#include <sys/stat.h>
#include <fstream>
#include <atomic>
#include <thread>
#include <mutex>
#include "meta_server.h"

const int physicalShiftBits = 18; 
const int maxRetryCount = 100;
const uint64_t updateTimestampStep = 30;
const uint64_t saveTimestampInterval = 1;
const uint64_t updateTimestampGuard = 1;
const int64_t maxLogical = int64_t(1 << physicalShiftBits);
const std::string path = "/home/t500ttt/RucDDBS/data/";
const std::string TIMESTAMP_FILE_NAME = "last_timestamp";

class Oracle
{
private:
    int64_t lastTS;
    // std::atomic<uint64_t> current_;
    struct timestamp
    {
        std::mutex mutex_;
        uint64_t physiacl;
        uint64_t logical;
    };
    // std::atomic<timestamp> current_;
    timestamp current_;
    
public:
    Oracle(){};
    ~Oracle(){};
    void start(){
        syncTimestamp();
        std::thread update([&]{
            while(1){
                updateTimestamp();
                std::this_thread::sleep_for(std::chrono::milliseconds(updateTimestampStep));
            }
        });
        update.join();
    }

    void getTimestampFromPath(){
        struct stat st; 
        if( ! (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ){
            throw MetaServerErrorException(MetaServerError::NO_META_DIR);
        }
        if (chdir(path.c_str()) < 0) {
            throw MetaServerErrorException(MetaServerError::UnixError);
        }
        //get TimeStamp
        std::ifstream ifs (TIMESTAMP_FILE_NAME);
        ifs >> lastTS; 
    }

    void saveTimeStampToPath(uint64_t ts){
        struct stat st; 
        if( ! (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ){
            throw MetaServerErrorException(MetaServerError::NO_META_DIR);
        }
        if (chdir(path.c_str()) < 0) {
            throw MetaServerErrorException(MetaServerError::UnixError);
        }
        //save TimeStamp
        std::ofstream ofs (TIMESTAMP_FILE_NAME);
        ofs << ts;
    }

    void syncTimestamp(){
        getTimestampFromPath();
        auto next = GetPhysical();
        if( next-lastTS < updateTimestampGuard){
            std::cout << "system time may be incorrect" << std::endl;
            next = lastTS + updateTimestampGuard;
        }
        uint64_t save = next + saveTimestampInterval;
        saveTimeStampToPath(save);
        std::cout << "sync and save timestamp" << std::endl;
        std::unique_lock<std::mutex> latch_(current_.mutex_);
        current_.physiacl = next;
        current_.logical = 0;
        // current_.store({next, 0} , std::memory_order_relaxed);
        // current_.store(ComposeTS(next, 0), std::memory_order_relaxed);
        return ;
    }

    void updateTimestamp(){
        std::unique_lock<std::mutex> latch_(current_.mutex_);
        uint64_t physical = current_.physiacl;
        uint64_t logic = current_.logical;
        
        auto now = GetPhysical();
        uint64_t jetLag = now - physical;
        int64_t next;
        if(jetLag > updateTimestampGuard){
            next = now;
        }else if(logic > maxLogical/2){
            next = physical+1;
        }else{return;}
        auto x = lastTS-next;
        if(lastTS-next<=(signed)updateTimestampGuard){
            uint64_t save = next + saveTimestampInterval;
            saveTimeStampToPath(save);
        }
        current_.physiacl = next;
        current_.logical = 0;
        return;
    }

    uint64_t getTimeStamp(){
        uint64_t res;
        // auto current = current_.load(std::memory_order_acquire);
        for(int i=0; i < maxRetryCount; i++){
            std::unique_lock<std::mutex> latch_(current_.mutex_);
            uint64_t physical = current_.physiacl;
            uint64_t logic = ++current_.logical;
            latch_.unlock();
            if(current_.physiacl == 0){
                std::cout << "we haven't synced timestamp ok, wait and retry." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }
            if(logic > maxLogical){
                std::cout << "logical part outside of max logical interval" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(updateTimestampStep));
                continue;
            }
            return ComposeTS(physical, logic);
        }
        return 0;
    }

    uint64_t ComposeTS(int64_t physical, int64_t logical){
        return (physical << physicalShiftBits + logical);
    }

    uint64_t ExtractPhysical(uint64_t ts) {
        return int64_t(ts >> physicalShiftBits); 
    }

    int64_t GetPhysical() { 
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::system_clock::now().time_since_epoch());
        return ms.count();
    }

    
};
