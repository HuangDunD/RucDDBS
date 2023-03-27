#pragma once

#include "head.h"

class node_conn{
public:
    brpc::Channel channel;
    brpc::ChannelOptions options;
};