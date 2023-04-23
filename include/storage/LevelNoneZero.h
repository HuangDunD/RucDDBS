#include <string>

#include "TableCache.h"
#include "TableBuilder.h"
#include "BlockCache.h"

class LevelNoneZero{
public:
    explicit LevelNoneZero(const std::string &dir, TableCache* table_cache, BlockCache *block_cache);
    
    // 禁用复制构造函数
    LevelNoneZero(const LevelNoneZero &) = delete;
    LevelNoneZero& operator=(const LevelNoneZero &) = delete;

    void merge(const TableMeta &table_meta, uint64_t no);
    // 查找key
    std::pair<bool, std::string> search(std::string key);

    inline uint64_t size() { return size_; }
private:
    std::string dir_;
    std::vector<TableMeta> ssts_;
    uint64_t size_;
    uint64_t num_entries_;

    TableCache *table_cache_;
    BlockCache *block_cache_;

    void save_meta() const;
};