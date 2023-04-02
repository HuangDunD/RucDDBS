#include <string>
#include <fstream>

#include "SkipList.h"
#include "gtest/gtest.h"
#include "TableBuilder.h"

TEST(TableBuilder_TEST, emtpy_test) {
    std::ofstream ofs("table_builder_test", std::ios::binary);

    TableBuilder tablebuilder(&ofs);

    EXPECT_EQ(0, tablebuilder.numEntries());
    ofs.close();
}

TEST(TableBuilder_TEST, simple_test) {
    std::ofstream ofs("table_builder_test", std::ios::binary);

    TableBuilder tablebuilder(&ofs);
    const int N = 1024;
    for(int i = 0; i < N; i++) {
        std::string key(i + 1, 'a');
        std::string value(i + 1, 'b');
        tablebuilder.add(key, value);
        EXPECT_EQ(i + 1, tablebuilder.numEntries());
    }
    tablebuilder.finish();
    ofs.close();

    // read
    std::ifstream ifs("table_builder_test", std::ios::binary);
    if(ifs.is_open()) {
        uint64_t index_offset, index_size;
        ifs.seekg(- sizeof(uint64_t) * 2, std::ios::end);
        ifs.read((char*)&index_offset, sizeof(uint64_t));
        ifs.read((char*)&index_size, sizeof(uint64_t));
        
    }
}


int main(int argc, char **argv) {
  printf("Running main() from %s\n", __FILE__);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();   
}