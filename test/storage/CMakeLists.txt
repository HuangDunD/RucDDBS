add_executable(correctness ./correctness.cpp)
target_link_libraries(correctness KV_STORAGE) 

add_executable(correct_gtest ./correct_gtest.cpp)
target_link_libraries(correct_gtest KV_STORAGE gmock_main gtest)
