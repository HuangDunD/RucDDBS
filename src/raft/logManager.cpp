#include "logManager.h"

using namespace raft;

raft::RaftLogManager::RaftLogManager(Storage *_storage)
    : storage(_storage)
    , firstIndex(_storage->getFirstIndex())
    , lastIndex(_storage->getLastIndex()){
    assert(firstIndex <= lastIndex);
    size_t entryNum = lastIndex - firstIndex + 1;
    log.reserve(entryNum);
    // std::cout<<"log entryNum="<<entryNum<<"\n";
    for(auto& entry : storage->getEntries()){
        putEntry(entry);
    }
    // std::cout<<"log size="<<log.size()<<"\n";
    assert(entryNum == log.size());
}

IndexTerm RaftLogManager::lastIndexinTerm(int startIndex,int _term) const{
    int index = std::min(startIndex,lastIndex);
    for(;index>=getFirstIndex();index--){
        if(getCurrentTerm(index) <= _term)
            break;
    }
    return {index,getCurrentTerm(index)};
}

// check log term and index
bool RaftLogManager::isUptoDate(int _index, int _term) const{
    int lastLogTerm = getLastTerm();
    if(lastLogTerm != _term)
        return lastLogTerm < _term;
    return lastIndex <= _index;
}

void RaftLogManager::append(int _term, const json::Value& _command){
    log.emplace_back(_term,_command);
    lastIndex++;

    auto entry = getJsonEntry(lastIndex);
    storage->prepareEntry(lastIndex,entry);
    storage->putLastIndex(lastIndex);
}

void RaftLogManager::overWrite(int _firstIndex, const json::Value& entries){
    assert(_firstIndex <= lastIndex + 1);
    log.resize(_firstIndex);
    for(const json::Value& entry: entries.getArray()){
        if(entries.getSize() > 0)
            // put
            storage->prepareEntry(firstIndex++,entry);
    }
    lastIndex = static_cast<int>(log.size() - 1);
    storage->putLastIndex(lastIndex);
}

json::Value RaftLogManager::getJsonEntries(int _firstIndex, int maxEntries) const{
    json::Value entries(json::TYPE_ARRAY);
    int _lastIndex = std::min(lastIndex, _firstIndex + maxEntries - 1);
    for(int i = _firstIndex; i <= _lastIndex;i++){
        auto jsonEntry = getJsonEntry(i);
        entries.addValue(jsonEntry);
    }
    return entries;
}

bool RaftLogManager::contain(int _index, int term) const{
    if(_index > lastIndex)
        return false;
    return log[_index].term == term;
}

json::Value RaftLogManager::getJsonEntry(int _index) const{
    json::Value entry(json::TYPE_OBJECT);
    entry.addMember("term",log[_index].term);
    entry.addMember("command",log[_index].command);
    return entry;
}

void RaftLogManager::putEntry(const json::Value& entry){
    int term = entry["term"].getInt32();
    auto& command = entry["command"];
    log.emplace_back(term,command);
}

void RaftLogManager::printAllLogs(){
    std::cout<<"log size="<<log.size()<<"\n";
    int index = 0;
    for(auto& entry : log){
        int term = entry.term;
        json::Value command = entry.command;
        json::StringWriteStream sws;
        json::Writer writer(sws);
        command.writeTo(writer);
        std::string_view view = sws.get();
        std::string entryData("");
        entryData.append(view.data(),view.size());
        std::cout<<"index="<<index++<<" term="<<term<<" command="<<entryData<<"\n";
    }
    return;
}