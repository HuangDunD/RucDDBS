#include <string>

#include "SkipList.h"
#include "gtest/gtest.h"

SkipList memtable{};

TEST(SKIPLIST_TEST, empty){
    EXPECT_EQ(false, memtable.contains("10"));
    EXPECT_EQ(false, memtable.del("10"));
}

TEST(SKIPLIST_TEST, small_test){
    const int N = 1024;
    
    // put and get
    for(int i = 0; i < N; i++){
        std::string key(i, 'a');
        std::string value(i, 'b');
        memtable.put(key, value);
        EXPECT_EQ(true, memtable.contains(key));
        EXPECT_EQ(std::pair(true, value), memtable.get(key));
    }
    EXPECT_EQ(N, memtable.size());

    // del
    for(int i = 0; i < N; i+=2){
        std::string key(i, 'a');
        EXPECT_EQ(true, memtable.del(key));
        EXPECT_EQ(false, memtable.contains(key));
    }
    EXPECT_EQ(N/2, memtable.size());
}

int main(int argc, char **argv) {
  printf("Running main() from %s\n", __FILE__);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();   
}