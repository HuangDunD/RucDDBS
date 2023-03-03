
#include <butil/logging.h>
#include <brpc/channel.h>
#include <butil/time.h>
#include "meta_service.pb.h"
#include "meta_server.h"

#define server "[fd15:4ba5:5a2b:1008:b63e:b3f7:ba07:5d71]:8001"

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
        // We will receive response synchronously, safe to put variables
        // on stack.
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
        usleep(1000 * 1000L);

    }

    LOG(INFO) << "MetaClient is going to quit";
    return 0;

}