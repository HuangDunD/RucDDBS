#include "timestamp.h"

//timestamp test
int main(){
    Oracle o;
    std::atomic<bool> oracle_background_running = true;
    std::thread update([&]{o.start(std::ref(oracle_background_running));});
    auto last = o.getTimeStamp();
    long long count = 0;
    while (1)
    {
        count++;
        auto now = o.getTimeStamp();
        if(now <= last) {
            std::cout << "now: " << now << "last:" << last << std::endl;
            std::cout << "error" << std::endl;
        }
        last = now;
        // std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        if(count % 10000 == 0){
            std::cout << count << std::endl;
        }
    }
    update.join();
    return 0;
}