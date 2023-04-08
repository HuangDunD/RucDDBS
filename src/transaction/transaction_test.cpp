#include "gtest/gtest.h"
#include "transaction_manager.h"
#include "transaction_manager_rpc.h"
#include "meta_server_rpc.h"
#include "execution_rpc.h"

#include <brpc/channel.h>
class TransactionTest : public ::testing::Test {
   public:
    std::unique_ptr<Lock_manager> lock_manager_;
    std::unique_ptr<LogStorage> log_storage_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<KVStore> kv_;
    std::unique_ptr<TransactionManager> transaction_manager_;

   public:
    void SetUp() override {
        ::testing::Test::SetUp();
        lock_manager_ = std::make_unique<Lock_manager>(true);
        log_storage_ = std::make_unique<LogStorage>("test_db");
        log_manager_ = std::make_unique<LogManager>(log_storage_.get());
        kv_ = std::make_unique<KVStore>();
        transaction_manager_ = std::make_unique<TransactionManager>(lock_manager_.get(), kv_.get(), log_manager_.get(), 
            ConcurrencyMode::TWO_PHASE_LOCKING);

        brpc::Channel channel;
        brpc::ChannelOptions options;
        options.timeout_ms = 100;
        options.max_retry = 3;
        
        if (channel.Init(FLAGS_META_SERVER_ADDR.c_str(), &options) != 0) {
            LOG(ERROR) << "Fail to initialize channel";
        }
        meta_service::MetaService_Stub stub(&channel);
        
        meta_service::RegisterRequest request;
        meta_service::RegisterResponse response;
        while (!brpc::IsAskedToQuit() && !response.register_ok()){
            brpc::Controller cntl;
            request.set_sayhello(true);
            stub.NodeRegister(&cntl, &request, &response, NULL);
            if(response.register_ok())
                std::cout << "Register success! ip: " << cntl.local_side() << std::endl;
        }

        std::thread rpc_thread([&]{
            //启动事务brpc server
            brpc::Server server;

            transaction_manager::TransactionManagerImpl trans_manager_impl(transaction_manager_.get());

            if (server.AddService(&trans_manager_impl, 
                                    brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
                LOG(ERROR) << "Fail to add service";
            }

            butil::EndPoint point;
            if (!FLAGS_SERVER_LISTEN_ADDR.empty()) {
                if (butil::str2endpoint(FLAGS_SERVER_LISTEN_ADDR.c_str() ,&point) < 0) {
                    LOG(ERROR) << "Invalid listen address:" << FLAGS_SERVER_LISTEN_ADDR;
                }
            } else {
                point = butil::EndPoint(butil::IP_ANY, FLAGS_SERVER_PORT);
            }

            brpc::ServerOptions options;
            
            if (server.Start(point,&options) != 0) {
                LOG(ERROR) << "Fail to start MetaServer";
            }

            server.RunUntilAskedToQuit();
        });
        rpc_thread.detach();
    }
};

TEST_F(TransactionTest, TransactionTest1){
    // 写入(key1,value1)
    // 定义一个空事务指针
    Transaction* txn1 = nullptr;
    transaction_manager_->Begin(txn1);

    txn1->set_is_distributed(true);
    txn1->get_distributed_node_set()->push_back(IP_Port{"[::0]", 8002});
    txn1->get_distributed_node_set()->push_back(IP_Port{"[::0]", 8003}); 

    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 100;
    options.max_retry = 3;

    // if (channel.Init("[::0]:8003", &options) != 0) {
    //     LOG(ERROR) << "Fail to initialize channel";
    // }
    // distributed_plan_service::RemotePlanNode_Stub stub_exec(&channel);
    // transaction_manager::TransactionManagerService_Stub stub_trans(&channel);

    // lock_manager_->LockTable(txn1, LockMode::INTENTION_EXCLUSIVE, 1);
    // lock_manager_->LockPartition(txn1, LockMode::EXLUCSIVE, 1 , 1);

    // kv_->put("key1", "value1");

}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return RUN_ALL_TESTS();
}