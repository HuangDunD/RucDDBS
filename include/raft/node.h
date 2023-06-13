// #pragma once

// #include <atomic>
// #include <memory>

// #include <tinyev/noncopyable.h>
// #include <tinyev/EventLoop.h>
// #include <tinyev/EventLoopThread.h>
// #include <jrpc/server/RpcServer.h>

// #include "callback.h"
// #include "configure.h"
// #include "utils.h"
// #include "raft.h"
// #include "peer.h"
// #include "raftservice.h"

// namespace raft{
//     /**
//      * @brief Raft节点类
//      */
//     class Node : ev::noncopyable{
//     public:
//          /**
//          * @brief Node构造函数，初始化Raft节点
//          * @param config Raft节点配置
//          * @param serverLoop Raft服务器事件循环
//          */
//         Node(const Config& config,ev::EventLoop* serverLoop);
//         /**
//          * @brief 启动Raft节点
//          */
//         void Start();
//         /**
//          * @brief 获取Raft节点当前状态
//          * @return 返回当前Raft状态
//          */
//         RaftState getState();
//         /**
//          * @brief 提交一个命令
//          * @param command 要提交的命令
//          * @return 返回ProposeResult，包含提交命令的结果
//          */
//         ProposeResult propose(const json::Value& command);

//         Storage* getStorage(){return raftInstance->getStorage();}

//         Log* getLogs(){return raftInstance->getLogs();}

//     private:
//         void startInLoop();

//         void RequestVote(const RequestVoteArgs& args,const RequestVoteDoneCallback& done);

//         void OnRequestVoteReply(int _peer,const RequestVoteArgs& args,const RequestVoteReply& reply);

//         void AppendEntries(const AppendEntriesArgs& args,const AppendEntriesDoneCallback& done);

//         void OnAppendEntriesReply(int _peer,const AppendEntriesArgs& args,const AppendEntriesReply& reply);

//         template<typename Task> void RunTaskInLoop(Task&& task);

//         template<typename Task> void QueueTaskInLoop(Task&& task);

//         template<typename Task> void RunTaskInLoopAndWait(Task&& task);
        
//         void AssertInLoop() const{
//             loop->assertInLoopThread();
//         }
        
//         void AssertStarted() const{
//             assert(started);
//         }
        
//         void AssertNotStarted() const{
//             assert(!started);
//         }
    
//     private:
//         // pointer
//         typedef std::unique_ptr<Raft> RaftPtr;
//         typedef std::unique_ptr<RaftPeer> RaftPeerPtr;
//         // raft peer list
//         typedef std::vector<RaftPeerPtr> RaftPeerList;

//         // server id and peer number
//         const int id;
//         const int peerNum;
//         // one server but many raft groups
//         // node id -> multi raft group id
//         // const std::vector<int> raft_group_ids;        

//         // 
//         std::atomic_bool started = false;

//         RaftPtr raftInstance;
//         RaftPeerList peers;

//         std::chrono::milliseconds tickInterval;
//         jrpc::RpcServer rpcServer;
//         RaftService raftService;
//         // 
//         ev::EventLoopThread loopThread;
//         ev::EventLoop* loop;
//     };
// }


