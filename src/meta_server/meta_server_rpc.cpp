#include "meta_server_rpc.h"

namespace meta_service{

void MetaServiceImpl::GetPartitionKey( google::protobuf::RpcController* cntl_base,
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

void MetaServiceImpl::GetPartitionLocation(google::protobuf::RpcController* cntl_base,
                      const PartitionLocationRequest* request,
                      PartitionLocationResponse* response,
                      google::protobuf::Closure* done){
                        
        brpc::ClosureGuard done_guard(done);
        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
        
        std::string db_name = request->db_name();
        std::string tab_name = request->tab_name();
        std::string partition_key_name = request->partition_key_name();
        if(request->partition_range_case()==request->kIntPartitionRange){
            int64_t min_range = request->int_partition_range().min_range();
            int64_t max_range = request->int_partition_range().max_range();
            auto map = response->mutable_pid_partition_location();
            std::unordered_map<partition_id_t,ReplicaLocation> repli_loc = meta_server_->
                getReplicaLocationList(db_name, tab_name, partition_key_name, min_range, max_range);
            for(auto iter : repli_loc){
                PartitionLocationResponse_ReplicaLocation tmp;
                tmp.set_ip_addr(iter.second.ip_addr_);
                tmp.set_port(iter.second.port_);
                (*map)[iter.first] = tmp;
            }
            return;
        }else if(request->partition_range_case()==request->kStringPartitionRange){
            std::string min_range = request->string_partition_range().min_range();
            std::string max_range = request->string_partition_range().max_range();
        }
    }

}

int main(){
    TabMetaServer tab_meta_server{1, "test_table", PartitionType::RANGE_PARTITION, "row1", ColType::TYPE_INT, 3};
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
    if (!FLAGS_listen_addr.empty()) {
        if (butil::str2endpoint(FLAGS_listen_addr.c_str() ,&point) < 0) {
            LOG(ERROR) << "Invalid listen address:" << FLAGS_listen_addr;
            return -1;
        }
    } else {
        point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
    }

    brpc::ServerOptions options;

    if (server.Start(point,&options) != 0) {
        LOG(ERROR) << "Fail to start MetaServer";
        return -1;
    }

    server.RunUntilAskedToQuit();
    return 0;
}