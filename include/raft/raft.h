// #pragma once
// #include <cassert>
// #include <memory>
// #include <vector>

// #include <jackson/Value.h>
// #include "peer.h"
// #include "configure.h"
// #include "utils.h"
// #include "storage.h"
// #include "callback.h"
// #include "logManager.h"
// #include "randomEngine.h"


// namespace raft{
//     class Raft : ev::noncopyable{
//     public:
//         Raft(const Config& _config,const std::vector<RaftPeer*>& _peers);

//         RaftState getState() const;

//         ProposeResult getPropose(const json::Value& command);

//         void requestVote(const RequestVoteArgs& args,RequestVoteReply& reply);

//         void onRequestVoteReply(int _peer,const RequestVoteArgs& args, const RequestVoteReply& reply);

//         void appendEntries(const AppendEntriesArgs& args,AppendEntriesReply& reply);

//         void onAppendEntriesReply(int _peer, const AppendEntriesArgs& args, const AppendEntriesReply& reply);
    
//         void debugOutput() const;

//         void tick();

//         Storage* getStorage() { 
//             return &this->storage;
//         }

//         Log* getLogs() { 
//             return &this->log; 
//         }

//     private:
//     // functions
//         enum Role{
//             LEADER,
//             CANDIDATE,
//             FOLLOWER,
//         };

//         void tickOnElection();

//         void tickOnHeartbeat();

//         void stepdownToFollower(int targetTerm);

//         void transToCandidate();

//         void transToLeader();

//         void onNewInputTerm(int _term);

//         void resetTimer();

//         void startRequestVote();

//         void startAppendEntries();

//         void applyLog();

//         bool isStandalone() const{
//             return peerNum == 1;
//         }

//         void setCurrentTerm(int _term);

//         void setVotedFor(int _votedFor);

//         const char* getRoleString() const {
//             return  role == LEADER ? "leader" :
//                     role == FOLLOWER ? "follower" :
//                     "candidate"; 
//         }
    

//     private:
//         // values
//         constexpr static int votedForNull = -1;
//         constexpr static int initTerm = 0;
//         constexpr static int initCommitIndex = 0;
//         constexpr static int initLastApplied = 0;
//         constexpr static int initMatchIndex = 0;
//         constexpr static int maxEntriesSend = 100;

//         // const int raft_group_id; // multi-raft

//         const int id;
//         const int peerNum;

//         Storage storage;
//         Role role = FOLLOWER;
//         int currentTerm = initTerm;
//         int votedFor = votedForNull;
//         int votesGet = 0;
        
//         RaftLogManager log;
//         int commitIndex = initCommitIndex;
//         int lastApplied = initLastApplied;

//         int timeElapsed = 0;
//         const int heartbeatTimeout;
//         const int electionTimeout;
//         int randomElectionTimeout = 0;
//         Random randomGenerator;

//         std::vector<int> nextIndex;
//         std::vector<int> matchIndex;

//         typedef std::vector<RaftPeer*> RaftPeerList;
//         const RaftPeerList peers;

//         ApplyMsgCallback applyMsgCallback;
//         SnapshotCallback snapshotCallback;
//     };
// }

// #endif


