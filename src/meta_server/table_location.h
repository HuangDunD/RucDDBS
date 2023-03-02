#include <vector>
#include <stdint.h>
#include "partition_location.h"

enum class DuplicateType : int64_t
{
  NOT_DUPLICATE = 0, //非复制表
  DUPLICATE,         //复制表
};

class PhyTableLocation {
public:
    PhyTableLocation(){};
    ~PhyTableLocation(){};
    PhyTableLocation(table_oid_t table_oid, DuplicateType duplicate_type, 
                        std::vector<PhyPartitionLocation> partiton_list):
              table_oid_(table_oid), duplicate_type_(duplicate_type), partiton_list_(std::move(partiton_list)){};

    inline table_oid_t get_table_id() const {return table_oid_;}

    inline DuplicateType get_duplicate_type() const { return duplicate_type_;}

    inline const std::vector<PhyPartitionLocation>& get_partition_list() const {return partiton_list_;}

private:

    table_oid_t table_oid_; //表id

    DuplicateType duplicate_type_; //是否为复制表

    std::vector<PhyPartitionLocation> partiton_list_; //表中各个分区所在的位置

};