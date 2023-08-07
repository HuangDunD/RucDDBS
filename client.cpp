#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
#include <string>
#include "engine.h"
#include "session.pb.h"
#include "transaction_manager.h"


DEFINE_string(protocol, "baidu_std", "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8005", "IP Address of server");
DEFINE_string(load_balancer, "", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 0x7fffffff, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)"); 
DEFINE_int32(interval_ms, 1000, "Milliseconds between consecutive requests");

int main(){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = FLAGS_timeout_ms;
    options.max_retry = FLAGS_max_retry;

    if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    session::Session_Service_Stub stub(&channel);
    int log_id = 0;
    txn_id_t txn_id = 0;
    while(1){
        /* code */
        session::SQL_Request request;
        session::SQL_Response response;
        brpc::Controller cntl;

        // request.set_sql_statement("create table A (col1 int, col2 int);");
        cout << "rucdb >> "; 
        std::string sql_str = "";
        std::string sql_buf;
        while(sql_str.back() != ';'){
            std::getline(std::cin, sql_buf);
            sql_str += sql_buf;
        }

        request.set_sql_statement(sql_str);
        request.set_txn_id(txn_id);
        cntl.set_log_id(log_id ++);

        cout <<"txn_id = " <<txn_id << endl;
        stub.SQL_Transfer(&cntl, &request, &response, NULL);
        
        // if(response.txn_id())
        txn_id = response.txn_id();
        
        cout << response.txn_id() << endl <<  response.txt() <<endl;
        if (!cntl.Failed()) {
            LOG(INFO) << "Received response from " << cntl.remote_side()
                << " to " << cntl.local_side()
                << " latency=" << cntl.latency_us() << "us";
        } else {
            LOG(WARNING) << cntl.ErrorText();
        }
        usleep(FLAGS_interval_ms * 1000L);
    }
    LOG(INFO) << "Client is going to quit";
    return 0;
}