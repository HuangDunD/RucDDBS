#pragma once
#include <vector>
#include <chrono>

#include <tinyev/InetAddress.h>
#include "callback.h"

namespace raft{
    // 1 node N group
    struct NodeConfig{

    };

    struct RaftGroupConfig{
    
    };



    /*
    Raft节点配置信息结构体
    */
    struct Config{
        // Raft节点的ID
        int id;

        // Raft group id
        int raft_group_id;

        // 用于持久化数据的LevelDB存储路径
        std::string storagePath;

        // 领导者发送心跳的时间间隔
        int heartbeatTimeout = 1;

        // 选举超时时间
        int electionTimeout = 5;

        // Raft节点的tick间隔时间
        std::chrono::milliseconds timeUnit {100};

        // 该节点的RPC服务地址
        ev::InetAddress serverAddress;

        // 记录所有集群节点的地址
        std::vector<ev::InetAddress> peerAddresses;

        // 应用新日志时的用户回调函数
        ApplyMsgCallback applyMsgCallback;

        // 安装快照时的用户回调函数
        SnapshotCallback snapshotCallback;
    };
}
