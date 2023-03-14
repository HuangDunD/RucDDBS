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

        std::string min_range = request->string_partition_range().min_range();
        std::string max_range = request->string_partition_range().max_range();
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

    }



//创建非分区表函数, 由meta_server分配table_oid, 并将Partition Type设置成None partition
void MetaServiceImpl::CreateTable(::google::protobuf::RpcController* controller,
                       const ::meta_service::CreateTableRequest* request,
                       ::meta_service::CreateTableResponse* response,
                       ::google::protobuf::Closure* done){

            brpc::ClosureGuard done_guard(done);
            std::shared_lock<std::shared_mutex> ms_latch(meta_server_->get_mutex());
            if(meta_server_->get_db_map().count(request->db_name())!=1){
                response->set_success(false);
                return;
            }
            std::unique_lock<std::shared_mutex> dms_latch(meta_server_->mutable_db_map()[request->db_name()]->get_mutex());
            ms_latch.unlock();

            if(meta_server_->mutable_db_map()[request->db_name()]->gettablemap().count(request->tab_name())==1){
                response->set_success(false);
                return;
            }else{
                auto tms = new TabMetaServer();
                meta_server_->mutable_db_map()[request->db_name()]->gettablemap()[request->tab_name()] = tms;

                tms->name = request->tab_name();
                tms->oid = meta_server_->mutable_db_map()[request->db_name()]->get_next_oid();
                meta_server_->mutable_db_map()[request->db_name()]->set_next_oid(tms->oid + 1);
                tms->partition_type = PartitionType::NONE_PARTITION;

                response->set_oid(tms->oid);
                response->set_success(true);
                return;
            }
    }

    //创建分区表
void MetaServiceImpl::CreatePartitionTable(::google::protobuf::RpcController* controller,
                       const ::meta_service::CreatePartitonTableRequest* request,
                       ::meta_service::CreatePartitonTableResponse* response,
                       ::google::protobuf::Closure* done){

            brpc::ClosureGuard done_guard(done);
            std::shared_lock<std::shared_mutex> ms_latch(meta_server_->get_mutex());

            if(meta_server_->get_db_map().count(request->db_name())!=1){
                response->set_success(false);
                return;
            }
            std::unique_lock<std::shared_mutex> dms_latch(meta_server_->mutable_db_map()[request->db_name()]->get_mutex());
            ms_latch.unlock();

            if(meta_server_->mutable_db_map()[request->db_name()]->gettablemap().count(request->tab_name())==1){
                response->set_success(false);
                return;
            }
            if(request->partition_type() == PartitionType_proto::NONE_PARTITION){
                response->set_success(false);
                return;
            }
            if(request->replica_cnt() < meta_server_->get_ip_node_map().size()){
                //副本数不能超过注册节点数
                response->set_success(false);
                return;
            }
            auto tms = new TabMetaServer();
            meta_server_->mutable_db_map()[request->db_name()]->gettablemap()[request->tab_name()] = tms;
            tms->name = request->tab_name();
            tms->oid = meta_server_->mutable_db_map()[request->db_name()]->get_next_oid();
            meta_server_->mutable_db_map()[request->db_name()]->set_next_oid(tms->oid + 1);

            tms->partition_key_name = request->partition_key_name();
            tms->partition_cnt_ = request->partition_cnt();

            switch (request->partition_key_type())
            {
            case PartitionType_proto::RANGE_PARTITION:
                tms->partition_type = PartitionType::RANGE_PARTITION;
                break;
            case PartitionType_proto::HASH_PARTITION :
                tms->partition_type = PartitionType::HASH_PARTITION;
                break;
            default:
                break;
            }
            switch (request->partition_key_type())
            {
            case ColType_proto::TYPE_INT :
                tms->partition_key_type = ColType::TYPE_INT;
                break;
            case ColType_proto::TYPE_STRING :
                tms->partition_key_type = ColType::TYPE_STRING;
                break;
            case ColType_proto::TYPE_FLOAT :
                tms->partition_key_type = ColType::TYPE_FLOAT;
                break;
            default:
                break;
            }
            
            for(int i=0; i<tms->partition_cnt_; i++){
                ParMeta tmp;
                tmp.name = "p" + std::to_string(i);
                tmp.p_id = i;
                
                tmp.string_range.min_range = request->range()[i].min_range();
                tmp.string_range.max_range = request->range()[i].max_range();
                tms->partitions.push_back(tmp);
            }
            
            tms->table_location_.set_table_id(tms->oid);
            if(request->has_replica()==true){
                tms->table_location_.set_duplicate_type(DuplicateType::DUPLICATE);
            }else{
                tms->table_location_.set_duplicate_type(DuplicateType::NOT_DUPLICATE);
            }
            
            //分配物理节点
            for(int i = 0; i<tms->partitions.size(); i++){
                std::vector<ReplicaLocation> replic_loc;
                int min_cnt = INT32_MAX;
                std::unordered_set<std::string> s;
                Node* min_leader_node;
                for(auto &node: meta_server_->get_ip_node_map()){
                    if(node.second->activate==true && node.second->leader_par_cnt<min_cnt){
                        min_cnt = node.second->leader_par_cnt;
                        min_leader_node = node.second;
                    }
                }
                replic_loc.push_back({min_leader_node->ip_addr,min_leader_node->port, Replica_Role::Leader});
                min_leader_node->leader_par_cnt++;
                s.emplace(min_leader_node->ip_addr);
                for(int j=0; j<request->replica_cnt()-1; j++){
                    Node* min_follower_node;
                    int min_cnt = INT32_MAX;
                    for(auto &node: meta_server_->get_ip_node_map()){
                        if(node.second->activate==true && s.count(node.second->ip_addr)==0 &&node.second->follower_par_cnt<min_cnt){
                            min_cnt = node.second->follower_par_cnt;
                            min_follower_node = node.second;
                        }
                    }
                    replic_loc.push_back({min_follower_node->ip_addr, min_follower_node->port, Replica_Role::Follower});
                    min_follower_node->follower_par_cnt++;
                    s.emplace(min_follower_node->ip_addr);
                }
                PhyPartitionLocation loc(tms->oid,tms->partitions[i].p_id,request->replica_cnt(),replic_loc);
                tms->table_location_.mutable_partition_list().push_back(loc);
            }
            
            response->set_oid(tms->oid);
            response->set_success(true);
            return;
    }
}



int main(){
    TabMetaServer tab_meta_server{1, "test_table", PartitionType::RANGE_PARTITION, "row1", ColType::TYPE_INT, 3};
    // std::vector<ParMeta> par_meta;

    tab_meta_server.partitions.push_back({"p0", 0, {"0","500"}});
    tab_meta_server.partitions.push_back({"p1", 1, {"501","1000"}});
    tab_meta_server.partitions.push_back({"p2", 2, {"1001","2000"}});

    std::vector<ReplicaLocation> t1;
    t1.push_back({"127.0.0.1", 8000, Replica_Role::Leader});
    PhyPartitionLocation loc1(1, 0, 1, t1);

    std::vector<ReplicaLocation> t2;
    t2.push_back({"127.0.0.1", 7999, Replica_Role::Leader});
    PhyPartitionLocation loc2(1, 1, 1, t2);

    std::vector<ReplicaLocation> t3;
    t3.push_back({"127.0.0.1", 7998, Replica_Role::Leader});
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
    try{
        meta_server.close_meta_server("/home/t500ttt/RucDDBS/data/");
    }
    catch(MetaServerErrorException& e)
    {
        std::cerr << e.GetInfo() << '\n';
    }

    auto new_meta_server = new MetaServer();
    new_meta_server->open_meta_server("/home/t500ttt/RucDDBS/data/");
    
    new_meta_server->close_meta_server("/home/t500ttt/RucDDBS/data/");

    return 0;
}