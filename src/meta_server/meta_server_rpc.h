
#include <butil/logging.h>
#include <brpc/server.h>
#include <gflags/gflags.h>
#include "meta_service.pb.h"
#include "meta_server.h"

// #define listen_addr "[2001:da8:9000:a835:f8c0:e0a3:cef5:bcde]"
// #define listen_addr "[2409:8a15:2044:f4f0:9c5c:be18:25b1:e768]"
// #define listen_addr "[fd15:4ba5:5a2b:1008:b63e:b3f7:ba07:5d71]"

DEFINE_int32(port, 8001, "TCP Port of this server");
DEFINE_string(listen_addr, "[::0]:8001", "Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");

namespace meta_service{
class MetaServiceImpl : public MetaService{
public:
    MetaServiceImpl(MetaServer *meta_server):meta_server_(meta_server) {};
    MetaServiceImpl() {};
    virtual ~MetaServiceImpl() {};

    virtual void GetPartitionKey( google::protobuf::RpcController* cntl_base,
                      const PartionkeyNameRequest* request,
                      PartionkeyNameResponse* response,
                      google::protobuf::Closure* done);

    virtual void GetPartitionLocation(google::protobuf::RpcController* cntl_base,
                      const PartitionLocationRequest* request,
                      PartitionLocationResponse* response,
                      google::protobuf::Closure* done);

private: 
    MetaServer *meta_server_;
};
}

