#include "storage.h"

using namespace raft;

namespace{
    const int initalTerm   =  0;
    const int votedForNull = -1;
    const int initalIndex  =  0;

    const std::string currentTermKey = " currentTerm";
    const std::string votedForKey    = " votedFor";
    const std::string firstIndexKey  = " firstIndex";
    const std::string lastIndexKey   = " lastIndex";

    //检查文件(所有类型,包括目录和文件)是否存在
    //返回1:存在 0:不存在
    int isFileExist(const char* path){
        return !access(path, F_OK);
    }

    // 解析slice
    json::Value parseSlice(const std::string& str){
        json::Document doc;
        std::string_view view(str.c_str());
        json::ParseError ret =  doc.parse(view);
        assert(ret == json::PARSE_OK); (void)ret;
        return doc;
    }
}

Storage::Storage(const std::string& _path){
    // using file system api
    const char* dbPath = _path.c_str();
    if(isFileExist(dbPath)){
        // Non-empty DB
        db = new KVStore_beta(_path);
        initDB(0);
    }else { // Empty DB
        db = new KVStore_beta(_path);
        initDB(1);
    }
}

Storage::~Storage(){
    delete db;
}

std::vector<json::Value> raft::Storage::getEntries() const
{
    // 获取key在[firstIndex, lastIndex]范围内的数据 
    // need getpar with iterator
    char first[11],last[11];
    snprintf(first, sizeof first, "%010d", firstIndex);
    snprintf(last, sizeof last, "%010d", lastIndex);
    std::cout<<"storage first = "<<first<<"\n";
    std::cout<<"storage last = "<<last<<"\n";
    // 暂时依次get
    std::vector<json::Value> vec;
    for(int i=firstIndex;i<=lastIndex;i++){
        std::pair<bool, std::string> result;
        char temp[11];
        snprintf(temp, sizeof temp, "%010d", i);
        //std::cout<<"temp="<<temp<<"\n";
        result = db->get(temp);
        if(result.first){
            // not empty
            vec.push_back(parseSlice(result.second));
            //std::cout<<"storage Key = "<<result.second<<"\n";
        }
    }
    assert(!vec.empty());
    return vec;
}

void Storage::initDB(bool _emptyDB){
    // none empty
    if(!_emptyDB){
        // CAN NOT BE NON-EMPTY
        std::cout<<"not empty DB"<<"\n";
        //DEBUG("raft[] not empty DB");
        std::string str =  get(currentTermKey);
        currentTerm = std::stoi(get(currentTermKey));
        votedFor = std::stoi(get(votedForKey));
        firstIndex = std::stoi(get(firstIndexKey));
        lastIndex = std::stoi(get(lastIndexKey));
        return;
    }
    else{
        // empty db
        std::cout<<"Empty DB"<<"\n";
        // DEBUG("raft[]  empty DB");
        // initalize
        currentTerm = initalTerm;
        votedFor = votedForNull;
        firstIndex = initalIndex;
        lastIndex = initalIndex;
        // value to store
        put(currentTermKey,std::to_string(currentTerm));
        put(votedForKey, std::to_string(votedFor));
        put(firstIndexKey, std::to_string(firstIndex));
        put(lastIndexKey, std::to_string(lastIndex));
        
        json::Value entry(json::TYPE_OBJECT);
        entry.addMember("term",initalTerm);
        entry.addMember("command","KVStore initialized");
        prepareEntry(initalIndex,entry);
    }
}

void Storage::put(const std::string& _key,const std::string& _value){
    db->put(_key,_value);
}

std::string Storage::get(const std::string& _key){
    std::pair<bool, std::string> result;
    result = db->get(_key);
    if(!result.first){
        // error
    }
    return result.second;
}

void Storage::putCurrentTerm(int _currentTerm){
    if(_currentTerm != currentTerm){
        currentTerm = _currentTerm;
        put(currentTermKey,std::to_string(_currentTerm));
    }
}

void raft::Storage::putVotedFor(int _votedFor){
    if(votedFor != _votedFor){
        votedFor = _votedFor;
        put(votedForKey,std::to_string(_votedFor));
    }
}

void Storage::putFirstIndex(int _firstIndex)
{
    if(_firstIndex != firstIndex){
        firstIndex = _firstIndex;
        put(firstIndexKey,std::to_string(_firstIndex));
    }
}

void Storage::putLastIndex(int _lastIndex){
    if(_lastIndex != lastIndex){
        lastIndex = _lastIndex;
        put(lastIndexKey,std::to_string(lastIndex));
    }
}

void raft::Storage::prepareEntry(int index, const json::Value &entry){
    char key[11];
    snprintf(key, sizeof key, "%010d", index);    

    json::StringWriteStream sws;
    json::Writer writer(sws);
    entry.writeTo(writer);
    std::string_view view = sws.get();

    std::string entryData("");
    entryData.append(view.data(),view.size());

    std::cout<<"entryData="<<entryData<<"\n";
    db->put(key,entryData);
}