#include <chrono>
#include <tinyev/Logger.h>
#include "include/raft/node.h"
#include <iostream>

using namespace std::chrono_literals;

void usage()
{
    printf("usage: ./raft id address1 address2...");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    if (argc < 3)
        usage();

    setLogLevel(LOG_LEVEL_DEBUG);

    int id = std::stoi(argv[1]);
    std::vector<ev::InetAddress>
            peerAddresses;

    if (id + 2 >= argc) {
        usage();
    }
    // store raft group peers' address
    for (int i = 2; i < argc; i++) {
        peerAddresses.emplace_back(std::stoi(argv[i]));
    }
    // cluster configurations
    raft::Config config;
    config.raft_group_id = 1;
    config.id = id;                                             // id
    config.storagePath = "./raft." + std::to_string(id);        // storage path
    config.heartbeatTimeout = 1;                                // heartbeat timeout * timeunit 
    config.electionTimeout = 5;                                 // electionT timeout
    config.timeUnit = 100ms;                                    // time unit
    config.serverAddress = peerAddresses[id];                   // local address
    config.peerAddresses = peerAddresses;                       // raft peer addrress
    config.applyMsgCallback = [&](const raft::ApplyMessage& msg) {
        assert(msg.command.getStringView() == "raft example");
        // DEBUG("raft[%d] callback function in applying log",id);
        // std::cout<<"raft[" << id << "] apply log callback"<<"\n";
    };
    config.snapshotCallback = [](const json::Value& snapshot) {
        // no snapshot
        FATAL("not implemented yet");
    };
    // 启动一个新线程运行rpc server
    ev::EventLoopThread loopThread;
    ev::EventLoop* raftServerLoop = loopThread.startLoop();
    raft::Node raftNode(config, raftServerLoop);
    // 主线程loop
    ev::EventLoop loop;
    // 定时器，每隔1s执行一次回调函数
    loop.runEvery(1s, [&](){
        auto ret = raftNode.getState();
        if (ret.isLeader) {
            // 相当于leader收到一个请求
            raftNode.propose(json::Value("raft example"));
        }
    });
    // 启动raft节点，时钟激励
    raftNode.Start();
    loop.loop();
}

