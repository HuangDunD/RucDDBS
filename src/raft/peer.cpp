// #include "raft.h"
// #include "peer.h"

// using namespace raft;

// RaftPeer::RaftPeer(int _peer, ev::EventLoop* _loop, const ev::InetAddress& _serverAddress)
//     : peer(_peer)
//     , loop(_loop)
//     , serverAddress(_serverAddress)
//     , rpcClient(new jrpc::RaftClientStub(loop,serverAddress)){
//         setConnectionCallback();
//     }

// RaftPeer::~RaftPeer() = default;

// void RaftPeer::start(){
//     assertInLoop();
//     rpcClient->start();
// }

// void RaftPeer::setConnectionCallback(){
//     // set 
//     rpcClient->setConnectionCallback(
//         [this](const ev::TcpConnectionPtr& con){
//             bool _connected = con->connected();
//             loop->runInLoop([=](){
//                 OnConnectionCallback(_connected);
//             });
//         });
// }

// void RaftPeer::OnConnectionCallback(bool _connected){
//     assertInLoop();
//     connected = _connected;
//     if(!connected){
//         rpcClient.reset(new jrpc::RaftClientStub(loop,serverAddress));
//         setConnectionCallback();
//         rpcClient->start();
//     }
// }

// void RaftPeer::requestVote(const RequestVoteArgs& args){
//     assertInLoop();
//     if(!connected)
//         return;
    
//     auto callback = [=](json::Value response,bool isError, bool timeout){
//         if(isError || timeout)
//             return;
//         int term = response["term"].getInt32();
//         bool voteGranted = response["voteGranted"].getBool();

//         RequestVoteReply reply;
//         reply.term = term;
//         reply.voteGranted = voteGranted;
//         requestVoteReply(peer,args,reply);
//     };
//     rpcClient->RequestVote(args.term,args.candidateId,args.lastLogIndex,args.lastLogTerm,std::move(callback));
// }

// void RaftPeer::appendEntries(const AppendEntriesArgs& args){
//     assertInLoop();

//     if(!connected)
//         return;
//     auto callback = [=](json::Value response, bool isError, bool timeout){
//         if(isError || timeout)
//             return;
//         int term = response["term"].getInt32();
//         bool success = response["success"].getInt32();
//         int expectIndex = response["expectIndex"].getInt32();
//         int expectTerm = response["expectTerm"].getInt32();

//         loop->runInLoop([=](){
//             AppendEntriesReply reply;
//             reply.term = term;
//             reply.success = success;
//             reply.expectIndex = expectIndex;
//             reply.expectTerm = expectTerm;
//             appendEntriesReply(peer, args, reply);
//         });
//     };
//     rpcClient->AppendEntries(args.term,args.prevLogIndex,args.prevLogTerm,args.entries,args.leaderCommit,std::move(callback));
// }