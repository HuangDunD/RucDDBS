#ifndef STORAGE_LEVEL_ZERO
#define STORAGE_LEVEL_ZERO

#include <string>
#include <cstdint>
#include <vector>
#include <mutex>

#include "SSTableId.h"
#include "SkipList.h"
#include "TableCache.h"
#include "BlockCache.h"
#include "SSTable.h"
#include "format.h"

// TODO table_cache
class LevelZero {
public:
  // explicit 
  explicit LevelZero(const std::string &dir, TableCache* table_cache, BlockCache *block_cache);
  // 禁用复制构造函数和复制赋值函数
  LevelZero(const LevelZero &) = delete;
  LevelZero &operator=(const LevelZero &) = delete;

  // 用memtable创建新的SSTable
  void add(const SkipList &memtable, uint64_t no);
  // 查找key
  std::pair<bool, std::string> search(std::string key);
  // 最后一个SSTable的元信息
  TableMeta back() const;
  // 返回弹出最后一个SSTable
  void pop_back();

  void clear();

  inline uint64_t size() { return ssts_.size(); }
  inline std::vector<TableMeta> sstables() { return ssts_; }
private:
  std::string dir_;
  std::vector<TableMeta> ssts_;
  uint64_t size_;
  uint64_t num_entries_;

  TableCache *table_cache_;
  BlockCache *block_cache_;

  std::mutex latch_;

  void save_meta() const;
};

#endif