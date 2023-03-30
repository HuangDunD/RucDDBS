option(STORAGE_TEST OFF)
option(SKIPLIST_TEST ON)

if(STORAGE_TEST)
    # storage test
    add_executable(correctness ./correctness.cpp)
    target_link_libraries(correctness KV_STORAGE) 
    # storage googletest
    add_executable(correct_gtest ./correct_gtest.cpp)
    target_link_libraries(correct_gtest KV_STORAGE gmock_main gtest)
endif()

# skiplist
if(SKIPLIST_TEST)
    add_executable(skiplist_gtest ./skiplist_gtest.cpp ${STORAGE_SRC}/skiplist.cpp)
    target_include_directories(${STORAGE_DIR})
    target_link_libraries(skiplist_gtest gmock_main gtest)
endif()
