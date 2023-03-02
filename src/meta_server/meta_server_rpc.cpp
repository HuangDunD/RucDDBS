
#include <butil/logging.h>
#include <brpc/server.h>
#include "meta_server.pb.h"

DEFINE_int32(port, 8001, "TCP Port of this server");
DEFINE_string(listen_addr, "", "Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");

namespace meta_server{
class MetaServiceImpl : public MetaService{
    MetaServiceImpl() {};
    virtual ~MetaServiceImpl() {};

    virtual void GetPartitionKey( google::protobuf::RpcController* cntl_base,
                      const PartionkeyNameRequest* request,
                      PartionkeyNameResponse* response,
                      google::protobuf::Closure* done){
        
        brpc::ClosureGuard(done);

        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
        
        LOG(INFO) << "Received request[log_id=" << cntl->log_id() 
            << "] from " << cntl->remote_side() 
            << " to " << cntl->local_side()
            << ": db_name: " << request->db_name() << "table name: " << request->tab_name();

        
    }
};
}
