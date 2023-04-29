#include <string>
#include <mutex>

#include "TableCache.h"
#include "TableBuilder.h"
#include "BlockCache.h"

class LevelNoneZero{
public:
    explicit LevelNoneZero(const std::string &dir, TableCache* table_cache, BlockCache *block_cache);
    
    // 禁用复制构造函数
    LevelNoneZero(const LevelNoneZero &) = delete;
    LevelNoneZero& operator=(const LevelNoneZero &) = delete;

    // void merge(const TableMeta &table_meta);
    void merge(const std::vector<TableMeta> &table_meta);
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
    uint64_t count_;

    TableCache *table_cache_;
    BlockCache *block_cache_;

    std::mutex latch_;
    void save_meta() const;
};