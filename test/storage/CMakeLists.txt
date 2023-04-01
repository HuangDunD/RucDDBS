option(STORAGE_TEST OFF)
option(SKIPLIST_TEST "SKIPLIST_TEST" ON)
option(BLOCKBUILDER_TEST "sstable's test" ON)

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
    message("build skiplist_test")
    add_executable(skiplist_gtest 
                        ${PROJECT_SOURCE_DIR}/test/storage/skiplist_gtest.cpp 
                        ${PROJECT_SOURCE_DIR}/src/storage/SkipList.cpp
                        ${PROJECT_SOURCE_DIR}/src/storage/Entry.cpp)
    target_include_directories(skiplist_gtest PRIVATE ${STORAGE_DIR})
    target_link_libraries(skiplist_gtest gmock_main gtest)
endif()

# block builder test
if(BLOCKBUILDER_TEST) 
    message("build block builder test")
    add_executable(blockbuilder_gtest
                        ${PROJECT_SOURCE_DIR}/test/storage/blockbuilder_gtest.cpp
                        ${PROJECT_SOURCE_DIR}/src/storage/BlockBuilder.cpp
                        ${PROJECT_SOURCE_DIR}/src/storage/Option.cpp)
    target_include_directories(blockbuilder_gtest PRIVATE ${STORAGE_DIR})
    target_link_libraries(blockbuilder_gtest gmock_main gtest)
endif()

