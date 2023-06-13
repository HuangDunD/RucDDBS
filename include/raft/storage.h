#pragma once

#include <string>
#include <iostream>
#include <unistd.h>
#include <assert.h>

#include <jackson/Value.h>
#include <jackson/Writer.h>
#include <jackson/StringWriteStream.h>
#include <jackson/Document.h>

#include "include/storage/KVStore_beta.h"

namespace raft{
class Storage{
public:
    explicit Storage(const std::string& _path);
    ~Storage();

    // 禁用复制构造函数
    Storage(const Storage &) = delete;
    Storage& operator=(const Storage &) = delete;

    void putCurrentTerm(int _currentTerm);

    void putVotedFor(int _votedFor); 

    void putFirstIndex(int _firstIndex);

    void putLastIndex(int _lastIndex);

    int getCurrentTerm() const {
        return currentTerm;
    }

    int getVotedFor() const { 
        return votedFor; }

    int getFirstIndex() const
    { return firstIndex; }

    int getLastIndex() const
    { return lastIndex; } 

    void prepareEntry(int index, const json::Value& entry);
    
    std::vector<json::Value> getEntries() const;

    void put(const std::string& _key,const std::string& value);

    std::string get(const std::string& _key);

private:
    void initDB(bool _emptyDB);
   

    int currentTerm;
    int votedFor;
    int firstIndex;
    int lastIndex;
    KVStore_beta *db = nullptr;
};

}