#include <string>

#include "KVStore.h"
#include "gtest/gtest.h"

KVStore storage = KVStore("./data");

TEST(StorageTest, single_value) { 
    // const std::string empty_string = "";
    EXPECT_EQ("", storage.get(1));
    storage.put(1, "ysm_data");
    EXPECT_EQ("ysm_data", storage.get(1));
    EXPECT_EQ(true, storage.del(1));
    EXPECT_EQ("", storage.get(1));
	EXPECT_EQ(false, storage.del(1));
}

int main(int argc, char **argv) {
  printf("Running main() from %s\n", __FILE__);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();   
}


