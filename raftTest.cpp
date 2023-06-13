#include "include/raft/storage.h"
#include "include/raft/logManager.h"
#include <jackson/Value.h>
#include <jackson/Value.h>
#include <jackson/Writer.h>
#include <jackson/StringWriteStream.h>
#include <jackson/Document.h>
#include <iostream>

int main(){
    // storage Test
    raft::Storage sto("./raft.0");
    int currentTerm = sto.getCurrentTerm();
    int firstIndex = sto.getFirstIndex();
    int votedFor = sto.getVotedFor();
    int lastIndex = sto.getLastIndex();
    std::cout<<"currentTerm="<<currentTerm<<"\n";
    std::cout<<"firstIndex="<<firstIndex<<"\n";
    std::cout<<"votedFor="<<votedFor<<"\n";
    std::cout<<"lastIndex="<<lastIndex<<"\n";
    std::string str = sto.get("0000000000");
    std::cout<<"str="<<str<<"\n";
    str = sto.get(" currentTerm");
    std::cout<<" currentTerm="<<str<<"\n";

    // Log Manager Test
    raft::RaftLogManager logTest(&sto);
    std::string tempStr1 = "2nd Command";
    json::Value val1(tempStr1.c_str());
    logTest.append(0,val1);
    std::string tempStr2 = "3rd Command";
    json::Value val2(tempStr2.c_str());
    logTest.append(0,val2);
    std::string tempStr3 = "4th Command";
    json::Value val3(tempStr3.c_str());
    logTest.append(0,val3);
    logTest.printAllLogs();

    currentTerm = sto.getCurrentTerm();
    firstIndex = sto.getFirstIndex();
    votedFor = sto.getVotedFor();
    lastIndex = sto.getLastIndex();
    std::cout<<"currentTerm="<<currentTerm<<"\n";
    std::cout<<"firstIndex="<<firstIndex<<"\n";
    std::cout<<"votedFor="<<votedFor<<"\n";
    std::cout<<"lastIndex="<<lastIndex<<"\n";
    return 0;
}