// #pragma once
// #include "callback.h"
// #include "utils.h"
// // #include <tinyev/noncopyable.h>
// // #include <tinyev/EventLoop.h>
// // #include <tinyev/InetAddress.h>
// #include <raft/RaftClientStub.h>

// namespace raft{
//     class RaftPeer: ev::noncopyable{
//     public:
//         RaftPeer(int _peer, ev::EventLoop* _loop, const ev::InetAddress& _serverAddress);

//         ~RaftPeer();

//         void start();

//         void requestVote(const RequestVoteArgs& args);

//         void appendEntries(const AppendEntriesArgs& args);

//         void setRequestVoteReplyCallback(const RequestVoteReplyCallback& callback){
//             requestVoteReply = callback;
//         }

//         void setAppendEntriesReplyCallback(const AppendEntriesReplyCallback& callback){
//             appendEntriesReply = callback;
//         }
//     private:
//         void assertInLoop(){
//             loop->assertInLoopThread();
//         }

//         void setConnectionCallback();

//         void OnConnectionCallback(bool connected);

//     private:
//         const int peer;
//         ev::EventLoop* loop;
//         ev::InetAddress serverAddress;
//         bool connected = false;

//         RequestVoteReplyCallback requestVoteReply;
//         AppendEntriesReplyCallback appendEntriesReply;

//         typedef std::unique_ptr<jrpc::RaftClientStub> ClientPtr;
//         ClientPtr rpcClient;
//     }; 
// }
