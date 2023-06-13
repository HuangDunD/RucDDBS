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
    std::string tempStr3 = "5th Command!";
    json::Value val3(tempStr3.c_str());

    raft::RaftLogManager logTest(&sto);
    logTest.append(0,val3);
    logTest.printAllLogs();
    lastIndex = sto.getLastIndex();
    std::cout<<"lastIndex="<<lastIndex<<"\n";
    return 0;
}