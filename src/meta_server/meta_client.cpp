
#include <butil/logging.h>
#include <brpc/channel.h>
#include <butil/time.h>
#include "meta_service.pb.h"
#include "meta_server.h"

// #define server "[2407:c080:801:fffe::7b3c:b07a]:8001"

#define server "[::1]:8001"

int main(){

    brpc::Channel channel;

    brpc::ChannelOptions options;
    options.timeout_ms = 100;
    options.max_retry = 3;

    if (channel.Init(server, &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    meta_service::MetaService_Stub stub(&channel);

    // Send a request and wait for the response every 1 second.
    int log_id = 0;
    while (!brpc::IsAskedToQuit()) {

        meta_service::PartionkeyNameRequest request;
        meta_service::PartionkeyNameResponse response;
        brpc::Controller cntl;

        request.set_db_name("test_db");
        request.set_tab_name("test_table");

        cntl.set_log_id(log_id ++);  

        // Because `done'(last parameter) is NULL, this function waits until
        // the response comes back or error occurs(including timedout).
        stub.GetPartitionKey(&cntl, &request, &response, NULL);
        if (!cntl.Failed()) {
            LOG(INFO) << "Received response from " << cntl.remote_side()
                << " to " << cntl.local_side()
                << ": " << response.partition_key_name() 
                << cntl.response_attachment() << ")"
                << " latency=" << cntl.latency_us() << "us";
        } else {
            LOG(WARNING) << cntl.ErrorText();
        }
        usleep(2000 * 1000L);

        brpc::Controller cntl2;
        meta_service::PartitionLocationRequest request2;
        meta_service::PartitionLocationResponse response2;
        request2.set_db_name("test_db");
        request2.set_tab_name("test_table");
        request2.set_partition_key_name(response.partition_key_name());
        request2.mutable_int_partition_range()->set_min_range(100);
        request2.mutable_int_partition_range()->set_max_range(600);

        cntl2.set_log_id(log_id ++);  
        stub.GetPartitionLocation(&cntl2, &request2, &response2, NULL);

        if (!cntl2.Failed()) {
            LOG(INFO) << "Received response from " << cntl2.remote_side()
                << " to " << cntl2.local_side()
                << ": " ;
            auto res = response2.pid_partition_location();
            for(auto x : res){
                LOG(INFO) << "partition id: " << x.first << 
                    " ip address: "<< x.second.ip_addr() 
                    << " port: "<< x.second.port();
            }
            LOG(INFO) << " latency=" << cntl2.latency_us() << "us";
        } else {
            LOG(WARNING) << cntl2.ErrorText();
        }
        usleep(2000 * 1000L);
    }

    LOG(INFO) << "MetaClient is going to quit";
    return 0;

}