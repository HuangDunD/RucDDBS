#include<iostream>
#include"op_etcd.h"
int main(){
    std::cout << etcd_put("/test/k3","a \n b") << std::endl;
    std::cout << etcd_get("/test/k3") << std::endl;
    std::cout << etcd_del("/test/k3") << std::endl;
    
    std::cout << etcd_get("/test/k3") << std::endl;
    std::cout << etcd_put("/test/k3","c") << std::endl;
    std::cout << etcd_put("/test/k3","d") << std::endl;
    std::cout << etcd_get("/test/k3") << std::endl;
    return 0;
}