#include "benchmark_txn.h"

#include "gtest/gtest.h"
#include <filesystem>
#include <brpc/channel.h>

DEFINE_string(SERVER_NAME, "", "Server NAME");

class BenchmarkTest : public ::testing::Test {
   public:
    std::unique_ptr<Lock_manager> lock_manager_;
    std::unique_ptr<LogStorage> log_storage_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<KVStore> kv_;
    std::unique_ptr<TransactionManager> transaction_manager_;
    std::unique_ptr<Benchmark_Txn> benchmark_txn_manager_;

   public:
    void SetUp() override {
        ::testing::Test::SetUp();

        lock_manager_ = std::make_unique<Lock_manager>(true);
        log_storage_ = std::make_unique<LogStorage>("benchmark_db");
        log_manager_ = std::make_unique<LogManager>(log_storage_.get());
        kv_ = std::make_unique<KVStore>(FLAGS_DIR, log_manager_.get());
        
        transaction_manager_ = std::make_unique<TransactionManager>(lock_manager_.get(), kv_.get(), log_manager_.get(), 
            ConcurrencyMode::TWO_PHASE_LOCKING);
        benchmark_txn_manager_ = std::make_unique<Benchmark_Txn>(transaction_manager_.get());

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
            request.set_server_name(FLAGS_SERVER_NAME);
            request.set_server_listen_port(FLAGS_SERVER_LISTEN_PORT);
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

            benchmark_service::BenchmarkServiceImpl benchmark_service_impl(transaction_manager_.get());
            if (server.AddService(&benchmark_service_impl, 
                                    brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
                LOG(ERROR) << "Fail to add service";
            }

            butil::EndPoint point;
            if (!FLAGS_SERVER_LISTEN_ADDR.empty()) {
                if (butil::str2endpoint(FLAGS_SERVER_LISTEN_ADDR.c_str(),FLAGS_SERVER_LISTEN_PORT, &point) < 0) {
                    LOG(ERROR) << "Invalid listen address:" << FLAGS_SERVER_LISTEN_ADDR;
                }
            } else {
                point = butil::EndPoint(butil::IP_ANY, FLAGS_SERVER_LISTEN_PORT);
            }

            brpc::ServerOptions options;
            
            if (server.Start(point,&options) != 0) {
                LOG(ERROR) << "Fail to start Server";
            }

            server.RunUntilAskedToQuit();
        });
        rpc_thread.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

TEST_F( BenchmarkTest, benchmark_test){
    // 设置测试时间为五分钟
    int testDurationInMinutes = 5;
    
    // 计算测试结束的时间点
    std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now() + std::chrono::minutes(testDurationInMinutes);
    
    std::atomic<int> total_transaction_cnt(0);
    int last_trans_cnt = 0;

    // 设置计时器，每隔5秒输出测试状态
    auto timer = std::chrono::steady_clock::now();
    std::thread res_ouput([&]{
        while (std::chrono::steady_clock::now() < endTime+std::chrono::seconds(2)) {
            if (std::chrono::steady_clock::now() - timer >= std::chrono::seconds(5)) {
                std::cout << "Test is running..." ;
                std::cout << "total_trasaction: " << total_transaction_cnt << "  current tps: " << (total_transaction_cnt-last_trans_cnt) / 5 << std::endl;
                last_trans_cnt = total_transaction_cnt;
                timer = std::chrono::steady_clock::now();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return;
    });

    // 创建测试工作线程的容器
    std::vector<std::thread> threads;
    for(int i=0; i<FLAGS_THREAD_NUM; i++){
        threads.push_back(std::thread([&]{
            while (std::chrono::steady_clock::now() < endTime) {
                // 生成一个事务并执行
                benchmark_txn_manager_->Generate(FLAGS_READ_RATIO);
                total_transaction_cnt++;
                // 每次创建线程后可以添加适当的延迟或休眠时间，模拟真实的负载
                // std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }));
    }
    // 等待所有线程执行完成
    for (std::thread& thread : threads) {
        thread.detach();
    }
    res_ouput.join();
    ASSERT_EQ("", "");
}

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    testing::InitGoogleTest(&argc, argv);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Init successfully" << std::endl;
    std::cout << "press any key to continue" << std::endl;
    getchar();

    return RUN_ALL_TESTS();
}