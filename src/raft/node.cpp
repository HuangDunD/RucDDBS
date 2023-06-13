// #include "node.h"

// using std::placeholders::_1;
// using std::placeholders::_2;
// using std::placeholders::_3;

// using namespace raft;

// namespace{

//     void checkConfig(const Config& config){

//     }
// }

// Node::Node(const Config& config,ev::EventLoop* serverLoop)
//     : id(config.id)
//     , peerNum(static_cast<int>(config.peerAddresses.size()))
//     , tickInterval(config.timeUnit)
//     , rpcServer(serverLoop,config.serverAddress)
//     , raftService(rpcServer)
//     , loop(loopThread.startLoop()){
//     // check config
//     checkConfig(config);
//     std::vector<RaftPeer*> rawPeers;
//     for(int i = 0; i < peerNum; i++){
//         auto tempPtr = new RaftPeer(i,loop,config.peerAddresses[i]);
//         rawPeers.push_back(tempPtr);
//         peers.emplace_back(tempPtr);
//     }
//     raftInstance = std::make_unique<Raft>(config,rawPeers);
//     for(auto peer : rawPeers){
//         peer->setRequestVoteReplyCallback(std::bind(&Node::OnRequestVoteReply,this,_1,_2,_3));
//         peer->setAppendEntriesReplyCallback(std::bind(&Node::OnAppendEntriesReply,this,_1,_2,_3));
//     }
//     raftService.setDoRequestVoteCallback(std::bind(&Node::RequestVote,this,_1,_2));
//     raftService.setDoAppendEntriesCallback(std::bind(&Node::AppendEntries,this,_1,_2));
// }

// void Node::Start(){
//     RunTaskInLoopAndWait([=](){
//         startInLoop();
//     });
// }

// void Node::startInLoop(){
//     AssertInLoop();
//     if(started.exchange(true))
//         return;
//     rpcServer.start();
//     for(int i=0;i<peerNum;i++){
//         if(i!=id){
//             peers[i]->start();
//         }
//     }
//     DEBUG("raft[%d] peerNum = %d starting...", id, peerNum);
//     // 3s print info
//     loop->runEvery(std::chrono::seconds(3),[this](){raftInstance->debugOutput(); });
//     // tick
//     loop->runEvery(tickInterval,[this](){raftInstance->tick();});
// }

// RaftState Node::getState(){
//     // must have started
//     AssertStarted();
//     RaftState state;
//     RunTaskInLoopAndWait([&,this](){
//         AssertStarted();
//         state = raftInstance->getState();
//     });
//     return state;
// }

// ProposeResult raft::Node::propose(const json::Value &command){
//     AssertStarted();
//     ProposeResult res;
//     RunTaskInLoopAndWait([&,this](){
//         AssertStarted();
//         res = raftInstance->getPropose(command);
//     });
//     return res;
// }

// void Node::RequestVote(const RequestVoteArgs &args, const RequestVoteDoneCallback &done)
// {
//     AssertStarted();
//     RunTaskInLoop([=](){
//         RequestVoteReply reply;
//         raftInstance->requestVote(args,reply);
//         done(reply);
//     });
// }

// void Node::OnRequestVoteReply(int peer,const RequestVoteArgs& args,const RequestVoteReply& reply){
//     AssertStarted();
//     RunTaskInLoop([=](){
//         raftInstance->onRequestVoteReply(peer,args,reply);
//     });
// }

// void Node::AppendEntries(const AppendEntriesArgs& args,const AppendEntriesDoneCallback& done){
//     AssertStarted();
//     RunTaskInLoop([=](){
//         AppendEntriesReply reply;
//         raftInstance->appendEntries(args,reply);
//         done(reply);
//     });
// }

// void Node::OnAppendEntriesReply(int peer,const AppendEntriesArgs& args,const AppendEntriesReply& reply){
//     AssertStarted();
//     RunTaskInLoop([=](){
//         raftInstance->onAppendEntriesReply(peer,args,reply);
//     });
// }

// template<typename Task> void Node::RunTaskInLoop(Task&& task){
//     loop->runInLoop(std::forward<Task>(task));
// }

// template<typename Task> void Node::RunTaskInLoopAndWait(Task&& task){
//     // need to latch
//     ev::CountDownLatch latch(1);
//     RunTaskInLoop([&,this](){
//         task();
//         latch.count();
//     });
//     latch.wait();
// }

