#include <string>
#include <filesystem>

#include "gtest/gtest.h"
#include "TableBuilder.h"
#include "Merger.h"

class MERGER_TEST : public ::testing::Test {
 protected:
  void SetUp() override {
    // 在测试之前设置
    if(std::filesystem::exists(dir)) {
      std::filesystem::remove_all(dir);
      // std::filesystem::remove(dir);
    }
    std::filesystem::create_directory(dir);
  }

  void TearDown() override {
    // 在测试之后做清理工作
  }

  void increase(std::string &s) {
    if(s.size() == 0) {
        s = "A";
    }
    size_t i = s.size() - 1;
    if(s[i] == 'Z') {
       s[i] = 'a';
    }else if(s[i] == 'z') {
       s.push_back('A');
    }else{
       s[i]++;
    }
  }

  const std::string dir = "./merge_test_data";
};

TEST_F(MERGER_TEST, simple_test) {
  // n表示SSTable文件数量，size表示SSTable文件的size
  const uint64_t n = 10;
  const size_t size = 1024;
  // 声明SSTableId
  SSTableId table_ids[n];
  for(uint64_t i = 0; i < n; i++) {
    table_ids[i] = SSTableId(dir, i);
  }
  // 获得keys
  std::string s = "";
  std::vector<std::string> keys;
  for(size_t i = 0; i < n * size; i++) {
    increase(s);
    keys.push_back(s);
  }
  //创建SSTable
  for(uint64_t i = 0; i < n; i++) {
    std::ofstream ofs(table_ids[i].name(), std::ios::binary);
    TableBuilder table_builder(&ofs);
    for(uint64_t j = i; j < n * size; j += n) {
      table_builder.add(keys[j], keys[j]);
    }
    EXPECT_EQ(size, table_builder.numEntries());
    table_builder.finish(table_ids[i]);
    ofs.close();
  }
  // 读取SSTable并创建iterator
  std::ifstream *table_files[n];
  SSTable *sstables[n];
  std::vector<std::unique_ptr<Iterator>> children;
  for(uint64_t i = 0; i < n ;i++) {
    table_files[i] = new std::ifstream(table_ids[i].name());
    sstables[i] = new SSTable(table_files[i], nullptr);
    children.push_back(sstables[i]->NewIterator());
  }
  // 创建Merger
  auto merger = NewMergingIterator(std::move(children));
  EXPECT_EQ(false, merger->Valid());
  // 正向遍历
  merger->SeekToFirst();
  for(uint64_t i = 0; i < n * size; i++) {
    EXPECT_EQ(true, merger->Valid());
    EXPECT_EQ(keys[i], merger->Key());
    EXPECT_EQ(keys[i], merger->Value());
    merger->Next();
  }
  EXPECT_EQ(false, merger->Valid());
  // 反向遍历
  merger->SeekToLast();
  for(int i = keys.size() - 1; i >= 0; i--) {
    EXPECT_EQ(true, merger->Valid());
    EXPECT_EQ(keys[i], merger->Key());
    EXPECT_EQ(keys[i], merger->Value());
    merger->Prev();
  }
  EXPECT_EQ(false, merger->Valid());

  // 处理
  for(uint64_t i = 0; i < n ;i++) {
    delete sstables[i];
    delete table_files[i];
  }
}

TEST_F(MERGER_TEST, repeated_test) {
  // n表示SSTable文件数量，size表示SSTable文件的size
  const uint64_t n = 10;
  const size_t size = 1024;
  // 声明SSTableId
  SSTableId Atable_ids[n], Btable_ids[n];
  for(uint64_t i = 0; i < n; i++) {
    Atable_ids[i] = SSTableId(dir, i);
    Btable_ids[i] = SSTableId(dir, n + i);
  }
  // 获得keys
  std::string s = "";
  std::vector<std::string> keys;
  for(size_t i = 0; i < n * size; i++) {
    increase(s);
    keys.push_back(s);
  }
  //创建SSTable A
  for(uint64_t i = 0; i < n; i++) {
    std::ofstream ofs(Atable_ids[i].name(), std::ios::binary);
    TableBuilder table_builder(&ofs);
    for(uint64_t j = i; j < n * size; j += n) {
      table_builder.add(keys[j], "A");
    }
    EXPECT_EQ(size, table_builder.numEntries());
    table_builder.finish(Atable_ids[i]);
    ofs.close();
  }
  //创建SSTable B
  for(uint64_t i = 0; i < n; i++) {
    std::ofstream ofs(Btable_ids[i].name(), std::ios::binary);
    TableBuilder table_builder(&ofs);
    for(uint64_t j = i; j < n * size; j += n) {
      table_builder.add(keys[j], "B");
    }
    EXPECT_EQ(size, table_builder.numEntries());
    table_builder.finish(Btable_ids[i]);
    ofs.close();
  }

  // 读取SSTable并创建iterator
  std::ifstream *Atable_files[n], *Btable_files[n];
  SSTable *Asstables[n], *Bsstables[n];
  std::vector<std::unique_ptr<Iterator>> children;
  for(uint64_t i = 0; i < n ;i++) {
    Atable_files[i] = new std::ifstream(Atable_ids[i].name());
    Asstables[i] = new SSTable(Atable_files[i], nullptr);
    Btable_files[i] = new std::ifstream(Btable_ids[i].name());
    Bsstables[i] = new SSTable(Btable_files[i], nullptr);
    children.push_back(Asstables[i]->NewIterator());
    children.push_back(Bsstables[i]->NewIterator());
  }
  // 创建Merger
  auto merger = NewMergingIterator(std::move(children));
  EXPECT_EQ(false, merger->Valid());
  // 正向遍历
  merger->SeekToFirst();
  for(uint64_t i = 0; i < n * size; i++) {
    EXPECT_EQ(true, merger->Valid());
    EXPECT_EQ(keys[i], merger->Key());
    EXPECT_EQ("A", merger->Value());
    merger->Next();
    EXPECT_EQ(true, merger->Valid());
    EXPECT_EQ(keys[i], merger->Key());
    EXPECT_EQ("B", merger->Value());
    merger->Next();
  }
  EXPECT_EQ(false, merger->Valid());
  // 反向遍历
  merger->SeekToLast();
  for(int i = keys.size() - 1; i >= 0; i--) {
    EXPECT_EQ(true, merger->Valid());
    EXPECT_EQ(keys[i], merger->Key());
    EXPECT_EQ("B", merger->Value());
    merger->Prev();
    EXPECT_EQ(true, merger->Valid());
    EXPECT_EQ(keys[i], merger->Key());
    EXPECT_EQ("A", merger->Value());
    merger->Prev();
  }
  EXPECT_EQ(false, merger->Valid());

  // 处理
  for(uint64_t i = 0; i < n ;i++) {
    delete Asstables[i];
    delete Atable_files[i];
    delete Bsstables[i];
    delete Btable_files[i];
  }
}

int main(int argc, char **argv) {
  printf("Running main() from %s\n", __FILE__);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();   
}
