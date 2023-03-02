
#include <butil/logging.h>
#include <brpc/server.h>
#include "meta_service.pb.h"
#include "meta_server.h"

// DEFINE_int32(port, 8001, "TCP Port of this server");
// DEFINE_string(listen_addr, "", "Server listen address, may be IPV4/IPV6/UDS."
//             " If this is set, the flag port will be ignored");
// DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
//              "read/write operations during the last `idle_timeout_s'");

#define listen_addr ""
#define port 8001

namespace meta_service{
class MetaServiceImpl : public MetaService{
public:
    MetaServiceImpl(MetaServer *meta_server):meta_server_(meta_server) {};
    MetaServiceImpl() {};
    virtual ~MetaServiceImpl() {};

    virtual void GetPartitionKey( google::protobuf::RpcController* cntl_base,
                      const PartionkeyNameRequest* request,
                      PartionkeyNameResponse* response,
                      google::protobuf::Closure* done){
        
        brpc::ClosureGuard done_guard(done);

        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
        
        LOG(INFO) << "Received request[log_id=" << cntl->log_id() 
            << "] from " << cntl->remote_side() 
            << " to " << cntl->local_side()
            << ": db_name: " << request->db_name() << "table name: " << request->tab_name();
        
        response->set_partition_key_name(meta_server_->getPartitionKey(request->db_name(),request->tab_name()));
    }

private: 
    MetaServer *meta_server_;
};
}

int main(){
    TabMetaServer tab_meta_server{1, "test_table", PartitionType::RANGE_PARTITION, "row1", 3};
    // std::vector<ParMeta> par_meta;

    tab_meta_server.partitions.push_back({"p0", 0, {0,500}});
    tab_meta_server.partitions.push_back({"p1", 1, {501,1000}});
    tab_meta_server.partitions.push_back({"p2", 2, {1001,2000}});

    std::vector<ReplicaLocation> t1;
    t1.push_back({"127.0.0.1", 8000, Replica_Role::INVALID_ROLE});
    PhyPartitionLocation loc1(1, 0, 1, t1);

    std::vector<ReplicaLocation> t2;
    t2.push_back({"127.0.0.1", 7999, Replica_Role::INVALID_ROLE});
    PhyPartitionLocation loc2(1, 1, 1, t2);

    std::vector<ReplicaLocation> t3;
    t3.push_back({"127.0.0.1", 7998, Replica_Role::INVALID_ROLE});
    PhyPartitionLocation loc3(1, 2, 1, t3);

    std::vector<PhyPartitionLocation> loc_list;
    loc_list.push_back(loc1);
    loc_list.push_back(loc2);
    loc_list.push_back(loc3);

    tab_meta_server.table_location_ = std::move(PhyTableLocation(1, DuplicateType::NOT_DUPLICATE, loc_list));

    std::unordered_map<std::string, TabMetaServer*> tabs;
    tabs["test_table"] = &tab_meta_server;
    DbMetaServer db_meta("test_db", tabs);

    std::unordered_map<std::string, DbMetaServer*> db_map;
    db_map["test_db"] = &db_meta;

    MetaServer meta_server(db_map);

    // std::cout << meta_server.getPartitionKey("test_db", "test_table");

    brpc::Server server;

    meta_service::MetaServiceImpl meta_service_impl(&meta_server);
    
    if (server.AddService(&meta_service_impl, 
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }

    butil::EndPoint point;
    if (strcmp(listen_addr,"") != 0) {
        if (butil::str2endpoint(listen_addr, &point) < 0) {
            LOG(ERROR) << "Invalid listen address:" << listen_addr;
            return -1;
        }
    } else {
        point = butil::EndPoint(butil::IP_ANY, port);
    }

    brpc::ServerOptions options;

    if (server.Start(point,&options) != 0) {
        LOG(ERROR) << "Fail to start EchoServer";
        return -1;
    }

    server.RunUntilAskedToQuit();
    return 0;
}