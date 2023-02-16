#include <iostream>
#include "Lock_manager.h"


auto Lock_manager::LockTable(Transaction *txn, LockMode lock_mode, const table_oid_t &oid) -> bool {
    
    //步骤一: 检查事务状态
    if(txn->get_state() != TransactionState::GROWING){
        txn->set_transaction_state(TransactionState::ABORTED);
        return false;
    }

    //步骤二: 得到l_id
    Lock_data_id l_id(oid, Lock_data_type::TABLE);

    //步骤三: 通过mutex申请全局锁表
    // latch_.lock();
    std::unique_lock<std::mutex> Latch(latch_);
    LockRequestQueue* request_queue = &lock_map_[l_id];
    std::unique_lock<std::mutex> queue_lock(request_queue->latch_);
    Latch.unlock();

    //步骤四: 查找当前事务是否已经申请了目标数据项上的锁，如果存在则根据锁类型进行操作，否则执行下一步操作

}

auto Lock_manager::LockPartition(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id) -> bool {

}

auto Lock_manager::LockRow(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool {

}

auto Lock_manager::UnLockTable(Transaction *txn,  const table_oid_t &oid) -> bool {
    Lock_data_id l_id(oid, Lock_data_type::ROW);
}

auto Lock_manager::UnLockPartition(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id ) -> bool {
    Lock_data_id l_id(oid, p_id, Lock_data_type::ROW);
}

auto Lock_manager::UnLockRow(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool {
    Lock_data_id l_id(oid, p_id, row_id, Lock_data_type::ROW);
}

auto Lock_manager::Unlock(Transaction *txn, const Lock_data_id &l_id) -> bool {
    if(txn->get_state() == TransactionState::GROWING){
        txn->set_transaction_state(TransactionState::SHRINKING);
    }
}

int main(){
    std::cout << "test" << std::endl;

}
