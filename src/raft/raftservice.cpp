// #include "node.h"
// #include <raft/raftservice.h>

// using namespace raft;
// using namespace jrpc;

// RaftService::RaftService(jrpc::RpcServer& server)
//         : RaftServiceStub(server){}

// void RaftService::RequestVote(int term,int candidateId,int lastLogIndex,int lastLogTerm,const UserDoneCallback& done){
//     RequestVoteArgs args;
//     args.term = term;
//     args.candidateId = candidateId;
//     args.lastLogIndex = lastLogIndex;
//     args.lastLogTerm = lastLogTerm;

//     doRequestVote(args,[=](const RequestVoteReply& reply){
//         json::Value value(json::TYPE_OBJECT);
//         value.addMember("term",reply.term);
//         value.addMember("voteGranted",reply.voteGranted);
//         done(std::move(value));
//     });
// }

// void RaftService::AppendEntries(int term,int prevLogIndex,int prevLogTerm,json::Value entries,int leaderCommit,const UserDoneCallback& done){
//     AppendEntriesArgs args;
//     args.term = term;
//     args.prevLogIndex = prevLogIndex;
//     args.prevLogTerm = prevLogTerm;
//     args.entries = std::move(entries);
//     args.leaderCommit = leaderCommit; 

//     doAppendEntries(args,[=](const AppendEntriesReply& reply){
//         json::Value value(json::TYPE_OBJECT);
//         value.addMember("term",reply.term);
//         value.addMember("success",reply.success);
//         value.addMember("expectIndex",reply.expectIndex);
//         value.addMember("expectTerm",reply.expectTerm);
//         done(std::move(value));
//     });
// }