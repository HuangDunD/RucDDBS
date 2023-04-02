#include <string>

#include "BlockBuilder.h"
#include "Option.h"
#include "gtest/gtest.h"

BlockBuilder block_builder;

TEST(BlockBuilderTest, empty_test) {
    EXPECT_EQ(true, block_builder.empty());
    EXPECT_EQ(sizeof(uint64_t) * 3, block_builder.estimated_size());
}


TEST(BlockBuilderTest, simple_test) { 
    const int N = 1024;
    uint64_t expect_size = sizeof(uint64_t) * 2;
    for(int i = 0; i < N; i++) {
        if(i % Option::RESTART_INTERVAL == 0) {
            expect_size += sizeof(uint64_t);
        }
        std::string key(i + 1, 'a');
        std::string value(i + 1, 'b');
        expect_size += key.size() + value.size() + sizeof(uint64_t) * 2;
        
        block_builder.add(key, value);

        EXPECT_EQ(expect_size, block_builder.estimated_size());
    }
}

TEST(BlockBuilderTest, repeat_test) { 
    block_builder.reset();
    const int N = 1024;
    uint64_t expect_size = sizeof(uint64_t) * 2;
    for(int i = 0; i < N; i++) {
        if(i % Option::RESTART_INTERVAL == 0) {
            expect_size += sizeof(uint64_t);
        }
        std::string key(i + 1, 'a');
        std::string value(i + 1, 'b');
        expect_size += key.size() + value.size() + sizeof(uint64_t) * 2;
        
        block_builder.add(key, value);

        EXPECT_EQ(expect_size, block_builder.estimated_size());
    }
}

int main(int argc, char **argv) {
  printf("Running main() from %s\n", __FILE__);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();   
}