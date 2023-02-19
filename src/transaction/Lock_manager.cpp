#include <iostream>
#include "Lock_manager.h"

//NOTE:
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
        // if(iter->lock_mode_==LockMode::INTENTION_SHARED && target_lock_mode == LockMode::EXLUCSIVE) return false;
        // if(iter->lock_mode_==LockMode::INTENTION_EXCLUSIVE && target_lock_mode == LockMode::SHARED) return false;
        // if(iter->lock_mode_==LockMode::INTENTION_EXCLUSIVE && target_lock_mode == LockMode::S_IX) return false;
        // if(iter->lock_mode_==LockMode::INTENTION_EXCLUSIVE && target_lock_mode == LockMode::EXLUCSIVE) return false;
        // if(iter->lock_mode_==LockMode::SHARED && target_lock_mode == LockMode::INTENTION_EXCLUSIVE) return false;
        // if(iter->lock_mode_==LockMode::SHARED && target_lock_mode == LockMode::S_IX) return false;
        // if(iter->lock_mode_==LockMode::SHARED && target_lock_mode == LockMode::EXLUCSIVE) return false;
        // if(iter->lock_mode_==LockMode::S_IX && target_lock_mode != LockMode::INTENTION_SHARED) return false;
        // if(iter->lock_mode_==LockMode::EXLUCSIVE) return false;
}

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

auto Lock_manager::checkQueueCompatible(const LockRequestQueue *request_queue, const LockRequest *request) -> bool {
    return true;
}

auto Lock_manager::LockTable(Transaction *txn, LockMode lock_mode, const table_oid_t &oid) -> bool {
    
    //步骤一: 检查事务状态
    if(txn->get_state() != TransactionState::GROWING){
        txn->set_transaction_state(TransactionState::ABORTED);
        throw TransactionAbortException (txn->get_txn_id(), AbortReason::LOCK_ON_SHRINKING);
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

    //步骤四: 
    //查找当前事务是否已经申请了目标数据项上的锁，
    //如果存在, 并且申请锁模式相同,返回申请成功
    //如果存在, 但与之锁的模式不同, 则准备升级, 并检查升级是否兼容
    auto target_lock_mode = lock_mode;
    for(auto &iter : request_queue->request_queue_){
        if( iter->txn_id_ == txn->get_txn_id() ){
            //如果当前事务正在或已经申请同等模式的锁
            if(iter->lock_mode_ == target_lock_mode && iter->granted_ == true){
                // 已经获取锁, 上锁成功
                return true; 
            }
            else if(iter->lock_mode_ == target_lock_mode && iter->granted_ == false){
                //正在申请锁, 等待这个请求申请成功后返回
                request_queue->cv_.wait(queue_lock, [&request_queue, &iter, &txn]{
                    return Lock_manager::checkQueueCompatible(request_queue, iter) || 
                        txn->get_state()==TransactionState::ABORTED;
                });
                iter->granted_ = true;
                return true; 
            }
            //如果锁的模式不同, 则需要对锁进行升级, 首先检查是否有正在升级的锁, 如果有, 则返回升级冲突
            else if(request_queue->upgrading_ == true){
                throw TransactionAbortException (txn->get_txn_id(), AbortReason::UPGRADE_CONFLICT);
                return false;
            }
            else if(Lock_manager::isUpdateCompatible(iter, lock_mode) == false){
                //如果没有冲突则检查是否锁兼容, 如果升级不兼容
                throw TransactionAbortException (txn->get_txn_id(), AbortReason::INCOMPATIBLE_UPGRADE);
            }else{
                //如果升级兼容, 则upgrade 
                request_queue->upgrading_ = true; 
                iter->lock_mode_ = target_lock_mode;
                iter->granted_ = false;
                request_queue->cv_.wait(queue_lock, [&request_queue, &iter, &txn]{
                    return Lock_manager::checkQueueCompatible(request_queue, iter) || 
                        txn->get_state()==TransactionState::ABORTED;
                });
                request_queue->upgrading_ = false;
                iter->granted_ = true;
                //TODO, 加入锁集
                txn->get_lock_set()->emplace(l_id);
                return true;
            } 
        }
    }

    //步骤五, 如果当前事务在请求队列中没有申请该数据项的锁, 则新建请求加入队列
    //检查是否可以上锁, 否则阻塞, 使用条件变量cv来实现
    LockRequest* lock_request = new LockRequest(txn->get_txn_id(), target_lock_mode); 
    request_queue->request_queue_.emplace_back(lock_request); 
    
    request_queue->cv_.wait(queue_lock, [&request_queue, &lock_request, &txn] {
                    return Lock_manager::checkQueueCompatible(request_queue, lock_request) || 
                        txn->get_state()==TransactionState::ABORTED;
                });
    lock_request->granted_ = true;
    //TODO, 加入锁集
    txn->get_lock_set()->emplace(l_id);

    return true;
}

auto Lock_manager::LockPartition(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id) -> bool {


    return true;
}

auto Lock_manager::LockRow(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool {

    return true;
}

auto Lock_manager::UnLockTable(Transaction *txn,  const table_oid_t &oid) -> bool {
    Lock_data_id l_id(oid, Lock_data_type::ROW);

    return true;
}

auto Lock_manager::UnLockPartition(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id ) -> bool {
    Lock_data_id l_id(oid, p_id, Lock_data_type::ROW);

    return true;
}

auto Lock_manager::UnLockRow(Transaction *txn, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool {
    Lock_data_id l_id(oid, p_id, row_id, Lock_data_type::ROW);

    return true;
}

auto Lock_manager::Unlock(Transaction *txn, const Lock_data_id &l_id) -> bool {
    if(txn->get_state() == TransactionState::GROWING){
        txn->set_transaction_state(TransactionState::SHRINKING);
    }

    return true;
}

int main(){
    std::cout << "test" << std::endl;

}
