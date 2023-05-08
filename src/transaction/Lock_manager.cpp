#include <iostream>
#include <mutex>
#include "Lock_manager.h"

std::atomic<bool> Lock_manager::enable_no_wait_;

// 锁相容矩阵
auto Lock_manager::isLockCompatible(const LockRequest *iter, const LockMode &target_lock_mode) -> bool {
        switch (target_lock_mode) {
            case LockMode::INTENTION_SHARED:
                if(iter->lock_mode_ == LockMode::EXLUCSIVE){
                    return false;
                }
                break;
            case LockMode::INTENTION_EXCLUSIVE:
                if(iter->lock_mode_ != LockMode::INTENTION_SHARED && iter->lock_mode_ != LockMode::INTENTION_EXCLUSIVE){
                    return false;
                }
                break;
            case LockMode::SHARED:
                if(iter->lock_mode_ != LockMode::INTENTION_SHARED && iter->lock_mode_ != LockMode::SHARED){
                    return false;
                }
                break;
            case LockMode::S_IX:
                if(iter->lock_mode_ != LockMode::INTENTION_SHARED) {
                    return false;
                }
                break;
            case LockMode::EXLUCSIVE:
                return false;
            default:
                return false;
            }
        return true;
}
// 锁升级矩阵
auto Lock_manager::isUpdateCompatible(const LockRequest *iter, const LockMode &upgrade_lock_mode) -> bool {
    switch (iter->lock_mode_) {
        case LockMode::INTENTION_SHARED:
            if(upgrade_lock_mode == LockMode::INTENTION_SHARED){
                return false;
            }
            break;
        case LockMode::SHARED:
            if(upgrade_lock_mode != LockMode::EXLUCSIVE && upgrade_lock_mode != LockMode::S_IX){
                return false;
            }
            break;
        case LockMode::INTENTION_EXCLUSIVE:
            if(upgrade_lock_mode != LockMode::EXLUCSIVE && upgrade_lock_mode != LockMode::S_IX){
                return false;
            }
            break;
        case LockMode::S_IX:
            if(upgrade_lock_mode != LockMode::EXLUCSIVE){
                return false;
            }
            break;
        case LockMode::EXLUCSIVE:
            return false;
            break;
        default:
            return false;
        }
    return true;
}

// 寻找锁请求队列中 相同事务是否已经加锁，如果需要锁升级，那么进行锁升级
auto Lock_manager::checkSameTxnLockRequest(Transaction *txn, LockRequestQueue *request_queue, const LockMode target_lock_mode, std::unique_lock<std::mutex> &queue_lock) -> int
{
 
}

auto Lock_manager::checkQueueCompatible(const LockRequestQueue *request_queue, const LockRequest *request) -> bool {
    
}

auto Lock_manager::LockTable(Transaction *txn, LockMode lock_mode, const table_oid_t &oid) -> bool {
    
    try{
        //步骤一: 检查事务状态

        //步骤二: 得到l_id
        Lock_data_id l_id(oid, Lock_data_type::TABLE);

        //步骤三: 通过mutex申请全局锁表

        //步骤四: 
        //查找当前事务是否已经申请了目标数据项上的锁，
        //如果存在, 并且申请锁模式相同,返回申请成功
        //如果存在, 但与之锁的模式不同, 则准备升级, 并检查升级是否兼容
        
        //步骤五, 如果当前事务在请求队列中没有申请该数据项的锁, 则新建请求加入队列
        //检查是否可以上锁, 否则阻塞, 使用条件变量cv来实现

        return true;
    }
    catch(TransactionAbortException &e)
    {
        txn->set_transaction_state(TransactionState::ABORTED);
        std::cerr << e.GetInfo() << '\n';
        return false;
    }
}

auto Lock_manager::LockPartition(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id) -> bool {
    
    try{
        //步骤一: 检查事务状态

        //步骤二: 得到l_id
        Lock_data_id l_id(oid, p_id, Lock_data_type::PARTITION);
        Lock_data_id parent_table_l_id(oid, Lock_data_type::TABLE);

        //步骤三 检查父节点状态
        if(lock_mode == LockMode::SHARED || lock_mode == LockMode::INTENTION_SHARED){
            //TODO: 检查父节点是否上IS锁

        } 
        else if(lock_mode == LockMode::EXLUCSIVE || lock_mode == LockMode::INTENTION_EXCLUSIVE || lock_mode == LockMode::S_IX){
            //TODO: 检查父节点是否上IX锁
            
        }
        
        //步骤四: 通过mutex申请全局锁表

        //步骤五: 
        //查找当前事务是否已经申请了目标数据项上的锁，
        //如果存在, 并且申请锁模式相同,返回申请成功
        //如果存在, 但与之锁的模式不同, 则准备升级, 并检查升级是否兼容
        
        //步骤六, 如果当前事务在请求队列中没有申请该数据项的锁, 则新建请求加入队列
        //检查是否可以上锁, 否则阻塞, 使用条件变量cv来实现

        return true;
    }
    catch(TransactionAbortException &e)
    {
        txn->set_transaction_state(TransactionState::ABORTED);
        std::cerr << e.GetInfo() << '\n';
        return false;
    }
}

auto Lock_manager::LockRow(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool {

    try{
        //步骤一: 检查事务状态

        //步骤二: 得到l_id
        Lock_data_id l_id(oid, p_id, Lock_data_type::PARTITION);
        Lock_data_id parent_table_l_id(oid, Lock_data_type::TABLE);

        //步骤三 检查父节点状态以及当前加锁模式
        
        //步骤四: 通过mutex申请全局锁表

        //步骤五: 
        //查找当前事务是否已经申请了目标数据项上的锁，
        //如果存在, 并且申请锁模式相同,返回申请成功
        //如果存在, 但与之锁的模式不同, 则准备升级, 并检查升级是否兼容
        
        //步骤六, 如果当前事务在请求队列中没有申请该数据项的锁, 则新建请求加入队列
        //检查是否可以上锁, 否则阻塞, 使用条件变量cv来实现


    }
    catch(TransactionAbortException &e)
    {
        txn->set_transaction_state(TransactionState::ABORTED);
        std::cerr << e.GetInfo() << '\n';
        return false;
    }
}

auto Lock_manager::UnLockTable(Transaction *txn,  const table_oid_t &oid) -> bool {

    return true;
}

auto Lock_manager::UnLockPartition(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id ) -> bool {

    return true;
}

auto Lock_manager::UnLockRow(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool {

    return true;
}

auto Lock_manager::Unlock(Transaction *txn, const Lock_data_id &l_id) -> bool {

}

auto Lock_manager::RunNoWaitDetection() -> bool{ 
    return true;
} 
