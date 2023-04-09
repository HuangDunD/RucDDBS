add_executable(transaction_test
    transaction_test.cpp 
    transaction_manager.cpp 
    Lock_manager.cpp 
    ../recovery/log_manager.cpp
    ../recovery/recovery_manager.cpp
    ../execution/execution_rpc.cpp
    ../execution/build/distributed_plan_service.pb.cc
    ../../meta_server/build/meta_service.pb.cc 
    ../../dbconfig.cc
    ${PROTO_SRC} ${PROTO_HEADER}
    
)

add_executable(transaction_test2
    transaction_test2.cpp 
    transaction_manager.cpp 
    Lock_manager.cpp 
    ../recovery/log_manager.cpp
    ../recovery/recovery_manager.cpp
    ../execution/execution_rpc.cpp
    ../execution/build/distributed_plan_service.pb.cc
    ../../meta_server/build/meta_service.pb.cc 
    ../../dbconfig.cc
    ${PROTO_SRC} ${PROTO_HEADER}
)

target_link_libraries(transaction_test ${BRPC_LIB} ${DYNAMIC_LIB} ${GTEST_BOTH_LIBRARIES})
target_link_libraries(transaction_test2 ${BRPC_LIB} ${DYNAMIC_LIB} ${GTEST_BOTH_LIBRARIES})