#include <fstream>
#include <filesystem>
#include <cassert>

#include "TableBuilder.h"
#include "snappy.h"

TableBuilder::TableBuilder(std::ofstream *file) 
                    : file_(file), offset_(0), size_(0), num_entries_(0), datablock_(), indexblock_(){

}

uint64_t TableBuilder::numEntries() const {
    return num_entries_;
}

// void TableBuilder::create_sstable(const SkipList *memtable) {
//     SkipList::Iterator iter(memtable);
//     iter.SeekToFirst();
//     while(iter.Valid()) {
//         add(iter.key(), iter.value());
//         iter.Next();
//     }
// }

void TableBuilder::add(const std::string &key, const std::string &value) {
    assert(file_->is_open());

    assert(key >= last_key_);
    last_key_ = key;
    datablock_.add(key, value);
    num_entries_++;
    if(datablock_.estimated_size() >= Option::BLOCK_SPACE) {
        flush();
    }
}

// 写data_block
void TableBuilder::flush() {
    assert(file_->is_open());
    if(datablock_.empty()) {
        return ;
    }
    BlockHandle data_handle = writeBlock(&datablock_);
    // 写index_block
    std::string s;
    data_handle.EncodeInto(s);
    indexblock_.add(last_key_, s);
}

BlockHandle TableBuilder::writeBlock(BlockBuilder *block) {
    assert(file_->is_open());
    // if compressed
    std::string block_content = block->finish();
    std::string compressed;
    if(Option::BLOCK_COMPRESSED) {
        snappy::Compress(block_content.data(), block_content.size(), &compressed);
    }else {
        compressed = block_content;
    }
    file_->write(compressed.data(), compressed.size());

    // increase offset
    size_ = compressed.size();
    BlockHandle block_handle(offset_, size_);
    offset_ += size_;
    block->reset();
    return block_handle;
    // if(block != &indexblock_) {
    //     // if not index block, then insert handle to indexblock
    //     indexblock_.add(last_key_, s);
    // }else {
    //     // if index block, insert handle finally
    //     file_->write(s.data(), s.size());
    // }

}

TableMeta TableBuilder::finish(const SSTableId &table_id) {
    assert(file_->is_open());
    // write least data first
    flush();
    // write index block
    BlockHandle index_handle = writeBlock(&indexblock_);
    // write footer;
    Footer footer;
    footer.set_meta_index_handle(BlockHandle(0, 0));
    footer.set_index_hanlde(index_handle);
    std::string s;
    footer.EncodeInto(s);
    file_->write(s.data(), s.size());

    return TableMeta{table_id, num_entries_, size_, std::string(""), last_key_};
}

TableMeta TableBuilder::create_sstable(const SkipList &memtable, const SSTableId & table_id) {
    std::ofstream ofs(table_id.name(), std::ios::binary);
    TableBuilder table_builder(&ofs);
    auto iter = SkipList::Iterator(&memtable);
    iter.SeekToFirst();

    assert(ofs.is_open());

    while(iter.Valid()) {
        assert(ofs.is_open());
        table_builder.add(iter.key(), iter.value());
        iter.Next();
    }
    auto table_meta = table_builder.finish(table_id);
    ofs.close();
    return table_meta;
}