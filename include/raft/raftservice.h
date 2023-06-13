// #pragma once

// #include <tinyev/EventLoop.h>
// #include <raft/RaftServiceStub.h>
// #include "callback.h"

// class RaftService: public jrpc::RaftServiceStub<RaftService>{
// public:
//     explicit RaftService(jrpc::RpcServer& server);
//     void setDoRequestVoteCallback(const raft::DoRequestVoteCallback& callback){
//         doRequestVote = callback;
//     }
//     void setDoAppendEntriesCallback(const raft::DoAppendEntriesCallback& callback){
//         doAppendEntries = callback;
//     }

//     void RequestVote(int term,int candidateId,int lastLogIndex,int lastLogTerm,const jrpc::UserDoneCallback& done);
//     void AppendEntries(int term,int prevLogIndex,int prevLogTerm,json::Value entries,int leaderCommit,const jrpc::UserDoneCallback& done);

// private:
//     raft::DoRequestVoteCallback doRequestVote;
//     raft::DoAppendEntriesCallback doAppendEntries;
// };

