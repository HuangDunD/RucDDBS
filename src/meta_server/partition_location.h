#include <string>

using table_oid_t = int32_t;
using partition_id_t = int32_t;

enum Replica_Role{
    INVALID_ROLE = 0,
    Leader,
    Follower
};

struct ReplicaLocation
{
    std::string ip_addr_;
    int32_t port_;
    Replica_Role role_;
};

class PhyPartitionLocation
{
public:
    inline table_oid_t get_table_id() const {return table_oid_;}

    inline partition_id_t get_partition_id() const {return p_id_;}

    inline int32_t get_replica_cnt() const {return replica_cnt_;}

    inline std::vector<ReplicaLocation>& get_replica_location_vec() {return repliaca_location_;}
    
    inline ReplicaLocation* get_replica_location(Replica_Role role){
        for(auto &replica_loc: repliaca_location_){
            if(replica_loc.role_ == role)
                return &replica_loc;
        }
        return nullptr;
    }

    PhyPartitionLocation(){};

    PhyPartitionLocation(table_oid_t table_oid, partition_id_t p_id, int32_t replica_cnt, 
                                        std::vector<ReplicaLocation> repliaca_location)
                :table_oid_(table_oid), p_id_(p_id), replica_cnt_(replica_cnt),repliaca_location_(std::move(repliaca_location)){};  

    ~PhyPartitionLocation(){};

private:
    table_oid_t table_oid_; //表id
    partition_id_t p_id_; //分区id
    int32_t replica_cnt_; //副本数
    std::vector<ReplicaLocation> repliaca_location_; //副本所在位置

};
