#include <fstream>
#include <filesystem>

#include "TableBuilder.h"

TableBuilder::TableBuilder(const std::ofstream *file) : file_(file){
    sizes_ = 0;
    num_entries_ = 0;
}

void TableBuilder::add(const std::string &key, const std::string &value) {
    
}