#include <butil/logging.h>
#include <brpc/server.h>
#include "meta_service.pb.h" 
#include "meta_server.h"

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
    
    virtual void CreateDataBase(::google::protobuf::RpcController* controller,
                       const ::meta_service::CreateDatabaseRequest* request,
                       ::meta_service::CreateDatabaseResponse* response,
                       ::google::protobuf::Closure* done){
                        
            brpc::ClosureGuard done_guard(done);

            if(meta_server_->get_db_map().count(request->db_name())==1){
                //db已经存在
                response->set_success(false);
            }else{
                meta_server_->mutable_db_map()[request->db_name()] = new DbMetaServer(request->db_name());
                response->set_success(true);
            }
    };

    //创建非分区表函数, 由meta_server分配table_oid, 并将Partition Type设置成None partition
    virtual void CreateTable(::google::protobuf::RpcController* controller,
                       const ::meta_service::CreateTableRequest* request,
                       ::meta_service::CreateTableResponse* response,
                       ::google::protobuf::Closure* done);
    
    //创建分区表
    virtual void CreatePartitionTable(::google::protobuf::RpcController* controller,
                       const ::meta_service::CreatePartitonTableRequest* request,
                       ::meta_service::CreatePartitonTableResponse* response,
                       ::google::protobuf::Closure* done);

    //节点注册
    virtual void NodeRegister(::google::protobuf::RpcController* controller,
                       const ::meta_service::RegisterRequest* request,
                       ::meta_service::RegisterResponse* response,
                       ::google::protobuf::Closure* done){

            brpc::ClosureGuard done_guard(done);

            brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
            if(meta_server_->mutable_ip_node_map().count(butil::ip2str(cntl->remote_side().ip).c_str())>0){
                meta_server_->mutable_ip_node_map()[butil::ip2str(cntl->remote_side().ip).c_str()]->port = cntl->remote_side().port;
                meta_server_->mutable_ip_node_map()[butil::ip2str(cntl->remote_side().ip).c_str()]->activate = true;
            }
            else{
                meta_server_->mutable_ip_node_map()[butil::ip2str(cntl->remote_side().ip).c_str()] = new Node(
                    butil::ip2str(cntl->remote_side().ip).c_str(), cntl->remote_side().port, true);
            }
    }

    //分片换主
    virtual void UpdateLeader(::google::protobuf::RpcController* controller,
                    const ::meta_service::UpdateLeaderRequest* request,
                    ::meta_service::UpdateLeaderResponse* response,
                    ::google::protobuf::Closure* done){
            brpc::ClosureGuard done_guard(done);
            brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
            std::string db_name = request->db_name();
            std::string tab_name = request->tab_name();
            int32_t par_id = request->par_id();
            std::string ip_addr = butil::ip2str(cntl->remote_side().ip).c_str();
            meta_server_->UpdatePartitionLeader(db_name, tab_name, par_id, ip_addr);
    }

    virtual void GetTimeStamp(::google::protobuf::RpcController* controller,
                       const ::meta_service::getTimeStampRequest* request,
                       ::meta_service::getTimeStampResponse* response,
                       ::google::protobuf::Closure* done){
            //注意 brpc::ClosureGuard done_guard(done) 这一行, 极易忘记!!!
            brpc::ClosureGuard done_guard(done);
            
            auto now = meta_server_->get_oracle().getTimeStamp();
            response->set_timestamp(now);
    }


private: 
    MetaServer *meta_server_;
};
}

