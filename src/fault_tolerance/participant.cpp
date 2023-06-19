#include "participant.h"
#include <brpc/channel.h>
#include <vector>
#include <future>

Participant::Participant(TransactionManager *transaction_maneger)
    :transaction_maneger_(transaction_maneger) {

    std::thread check_txn([&]{
        std::shared_lock<std::shared_mutex> l(transaction_maneger_->txn_map_mutex);
        brpc::ChannelOptions options;
        options.timeout_ms = 100;
        options.max_retry = 3;

        for(auto &txn: transaction_maneger_->txn_map){
            if(txn.second->get_state() != TransactionState::PREPARED){
                continue;
            }
            //当前事务收到prepare请求，进入prepared状态
            brpc::Channel channel;
            if (channel.Init(txn.second->get_coor_ip().ip_addr.c_str(), txn.second->get_coor_ip().port, &options) != 0) {
                LOG(ERROR) << "Fail to initialize channel";
                continue;
            }
            fault_tolerance_service::FaultToleranceService_Stub stub(&channel);
            brpc::Controller cntl;
            fault_tolerance_service::CoordinatorStatusRequest request;
            fault_tolerance_service::CoordinatorStatusResponse response;

            stub.getCoordinatorStatus(&cntl, &request, &response, NULL);
            if(response.activate() == false){
                // 与协调者失联，尝试与所有参与者联系
                std::vector<std::future<bool>> futures_connection;
                for(auto node: *txn.second->get_distributed_node_set()){
                    futures_connection.push_back(std::async(std::launch::async, [&txn, node, &options]()->bool{
                        brpc::Channel channel;
                        if (channel.Init(node.ip_addr.c_str(), node.port, &options) != 0) {
                            LOG(ERROR) << "Fail to initialize channel";
                            return false;
                        }
                        fault_tolerance_service::FaultToleranceService_Stub stub(&channel);
                        brpc::Controller cntl;
                        fault_tolerance_service::ParticipantStatusRequest request;
                        fault_tolerance_service::ParticipantStatusResponse response;

                        stub.getParticipantStatus(&cntl, &request, &response, NULL);
                    }) );
                }
                //检查和各个参与者的连接情况
                bool new_coor_flag = true;
                for(size_t i=0; i<(*txn.second->get_distributed_node_set()).size(); i++){
                    if(futures_connection[i].get() == false){
                        new_coor_flag = false;
                        break;
                    }
                }
                if( new_coor_flag ){
                    // 选定新的协调者
                    brpc::Channel channel_get_new_coor;
                    if (channel_get_new_coor.Init(FLAGS_SERVER_LISTEN_ADDR.c_str(), FLAGS_SERVER_LISTEN_PORT, &options) != 0) {
                        LOG(ERROR) << "Fail to initialize channel";
                        continue;
                    }
                    meta_service::MetaService_Stub stub_metaserver(&channel_get_new_coor);
                    brpc::Controller cntl;
                    meta_service::getNewCoorRequest newcoor_request;
                    meta_service::getNowCoorResponse newcoor_response;
                    newcoor_request.set_fault_ip(txn.second->get_coor_ip().ip_addr.c_str());
                    newcoor_request.set_port(txn.second->get_coor_ip().port);
                    stub_metaserver.GetNewCoordinator(&cntl, &newcoor_request, &newcoor_response, NULL);
                    std::string coor_ip = newcoor_response.new_coor_ip();
                    int coor_port = newcoor_response.port();

                    brpc::Channel channel;
                    if (channel.Init(coor_ip.c_str(), coor_port, &options) != 0) {
                        LOG(ERROR) << "Fail to initialize channel";
                        continue;
                    }
                    fault_tolerance_service::FaultToleranceService_Stub stub(&channel);
                    fault_tolerance_service::NewCoordinatorRequest request;
                    fault_tolerance_service::NewCoordinatorResponse response;
                    cntl.Reset();

                    request.set_txn_id(txn.first);
                    for(auto x: *txn.second->get_distributed_node_set()){
                        auto ip = request.add_ips();
                        ip->set_ip(x.ip_addr);
                        ip->set_port(x.port);
                    }
                    stub.NewCoordinator(&cntl, &request, &response, NULL);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    });
    check_txn.detach();

};