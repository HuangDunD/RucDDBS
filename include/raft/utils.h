#pragma once

#include <stdlib.h>
#include <string>
#include <set>

namespace raft{
    
    struct RequestVoteArgs{
        int term = -1;
        int candidateId = -1;
        int lastLogIndex = -1;
        int lastLogTerm = -1;
    };

    struct RequestVoteReply{
        int term = -1;
        bool voteGranted = false;
    };

    struct AppendEntriesArgs{
        int term = -1;
        int prevLogIndex = -1;
        int prevLogTerm = -1;
        std::string entries;
        int leaderCommit = -1;
    };

    struct AppendEntriesReply{
        int term = -1;
        bool success = false;
        int expectIndex = -1;
        int expectTerm = -1;
    };

    struct ProposeResult{
        int expectIndex = -1;
        int currentTerm = -1;
        bool isLeader = false;
    };

    struct RaftState{
        int currentTerm = -1;
        bool isLeader = false;
    };

    struct IndexTerm{
        int index;
        int term;
    };

    struct ApplyMessage{
        ApplyMessage(int _index,const std::string& _command)
            : index(_index),command(_command){}

        int index;
        std::string command;
    };

}