#pragma once

#include "utils.h"
#include "storage.h"

#include <jackson/Value.h>
#include <memory>
#include <vector>

namespace raft{
    class RaftLogManager {
    public:
        explicit RaftLogManager(Storage* _storage);

        RaftLogManager(const RaftLogManager &) = delete;

        RaftLogManager& operator=(const RaftLogManager &) = delete;

        int getFirstIndex() const{
            return firstIndex;
        }

        int getFirstTerm() const{
            return log[firstIndex].term;
        }

        int getLastIndex() const{
            return lastIndex;
        }

        int getLastTerm() const{
            return log[lastIndex].term;
        }

        int getCurrentTerm(int _index) const{
            return log[_index].term;
        }

        const json::Value&  getCurrentCommand(int _index) const{
            return log[_index].command;
        }

        IndexTerm lastIndexinTerm(int startIndex,int _term) const;

        bool isUptoDate(int _index,int _term) const;

        void append(int _term,const json::Value& _command);

        void overWrite(int _firstIndex,const json::Value& entries);

        json::Value getJsonEntries(int _firstIndex, int _maxEntries) const;
        
        bool contain(int _index, int _term) const;

        void printAllLogs();

    private:

        json::Value getJsonEntry(int _index) const;

        void putEntry(const json::Value& entry);

    private:
        struct Entry{
            Entry(int _term,const json::Value& _command)
                : term(_term),command(_command) {}
            
            Entry() : term(0),command(json::TYPE_NULL){}
            
            int term;
            json::Value command;
        };
        
        Storage* storage;
        int firstIndex;
        int lastIndex;
        std::vector<Entry> log;
    };
}