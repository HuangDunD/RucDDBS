#include "Option.h"

const bool Option::BLOCK_COMPRESSED = true;

const uint64_t Option::BLOCK_SPACE = (uint64_t) 4 * 1024;
const uint64_t Option::SSTABLE_SPACE = (uint64_t)2 * 1024 * 1024;
const uint64_t Option::TABLE_CACHE_SIZE = 20;
const uint64_t Option::BLOCK_CACHE_SIZE = 100;
const uint64_t Option::RESTART_INTERVAL = 20;
const uint64_t Option::NZ_NUM = 6;

const uint64_t Option::Z_SIZE = 2;
const uint64_t Option::NZ_SIZE[] = {10, 100, 1000, 10000, 100000, 1000000};
const std::string Option::NAME_Z = "level_0";
const std::string Option::NZ_NAMES [] = {"level_1","level_2", "level_3", "level_4", "level_5", "level_6"};