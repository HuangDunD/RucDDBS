#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include "session.pb.h"
#include "transaction_manager_rpc.h"
#include "server.h"
#include "engine.h"

DEFINE_int32(port, 8005, "TCP Port of this server");
DEFINE_string(listen_addr, "", "Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_string(store_path, "/home/t500ttt/RucDDBS/data1", "server1 store");


int main(int argc, char **argv){
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    auto lock_manager_ = std::make_unique<Lock_manager>(true);
    auto log_storage_ = std::make_unique<LogStorage>("test_db");
    auto log_manager_ = std::make_unique<LogManager>(log_storage_.get());

    auto kv_ = std::make_unique<KVStore>(LOG_DIR,log_manager_.get());
    auto transaction_manager_sql = std::make_unique<TransactionManager>(lock_manager_.get(),kv_.get(),log_manager_.get(),ConcurrencyMode::TWO_PHASE_LOCKING);

    brpc::Server server;
    session::Session_Service_Impl session_service_impl(transaction_manager_sql.get());
    transaction_manager::TransactionManagerImpl trans_manager_impl(transaction_manager_sql.get());

    if (server.AddService(&session_service_impl, 
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }
    
    if (server.AddService(&trans_manager_impl, 
                            brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
    }
    
    butil::EndPoint point;
    if (!FLAGS_listen_addr.empty()) {
        if (butil::str2endpoint(FLAGS_listen_addr.c_str(), &point) < 0) {
            LOG(ERROR) << "Invalid listen address:" << FLAGS_listen_addr;
            return -1;
        }
    } else {
        point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
    }

    // 对当前节点做一些操作
    // Start the server.
    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s;
    if (server.Start(point, &options) != 0) {
        LOG(ERROR) << "Fail to start EchoServer";
        return -1;
    }

    // Wait until Ctrl-C is pressed, then Stop() and Join() the server.
    server.RunUntilAskedToQuit();
    return 0;
}