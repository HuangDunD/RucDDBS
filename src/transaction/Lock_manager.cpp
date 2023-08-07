#include <iostream>
#include <mutex>
#include "Lock_manager.h"

std::atomic<bool> Lock_manager::enable_no_wait_;


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

/// @brief 用于对同一个事务上的加锁请求进行锁升级的兼容性判断
/// @param iter 锁请求的指针，加锁请求的事务id=该指针的事务id
/// @param upgrade_lock_mode 新来的加锁模式
/// @param return_lock_mode 返回需要更新的锁请求模式，例如IX + S -> SIX
/// @return 是否需要锁升级，如果返回为false，则不需要更改iter
bool Lock_manager::isUpdateCompatible(const LockRequest *iter, const LockMode &upgrade_lock_mode, LockMode* return_lock_mode) {
    switch (iter->lock_mode_) {
        case LockMode::INTENTION_SHARED:
            if(upgrade_lock_mode == LockMode::INTENTION_SHARED){
                return false;
            }
            // IS + IX = IX, IS + S = S, IS + X = X
            *return_lock_mode = upgrade_lock_mode;
            break;
        case LockMode::SHARED:
            if(upgrade_lock_mode == LockMode::EXLUCSIVE) {
                // S + X = X
                *return_lock_mode = upgrade_lock_mode;
            }else if(upgrade_lock_mode == LockMode::INTENTION_EXCLUSIVE){
                // S + IX = SIX
                *return_lock_mode = LockMode::S_IX;
            }else{
                return false;
            }
            break;
        case LockMode::INTENTION_EXCLUSIVE:
            if(upgrade_lock_mode == LockMode::EXLUCSIVE) {
                // IX + X = X
                *return_lock_mode = upgrade_lock_mode;
            }else if(upgrade_lock_mode == LockMode::INTENTION_SHARED){
                // IX + S= SIX
                *return_lock_mode = LockMode::S_IX;
            }else{
                return false;
            }
            break;
        case LockMode::S_IX:
            if(upgrade_lock_mode != LockMode::EXLUCSIVE){
                return false;
            }
            // SIX + X = X
            *return_lock_mode = upgrade_lock_mode;
            break;
        case LockMode::EXLUCSIVE:
            return false;
            break;
        default:
            return false;
        }
    return true;
}
auto Lock_manager::checkSameTxnLockRequest(Transaction *txn, LockRequestQueue *request_queue, const LockMode target_lock_mode, std::unique_lock<std::mutex> &queue_lock) -> int
{
    for(auto &iter : request_queue->request_queue_){
        if( iter->txn_id_ == txn->get_txn_id() ){
            LockMode return_lock_mode;
            bool need_upgrade = isUpdateCompatible(iter, target_lock_mode, &return_lock_mode);
            // 如果当前事务正在或已经申请模式相同或更低级的锁
            if(need_upgrade == false && iter->granted_ == true){
                // 已经获取锁, 上锁成功
                return true; 
            }
            else if(need_upgrade == false && iter->granted_ == false){
                //正在申请锁, 等待这个请求申请成功后返回
                request_queue->cv_.wait(queue_lock, [&request_queue, &iter, &txn]{
                    //TODO deadlock
                    if(Lock_manager::enable_no_wait_==false)
                        return Lock_manager::checkQueueCompatible(request_queue, iter) || 
                            txn->get_state()==TransactionState::ABORTED;
                    if(Lock_manager::checkQueueCompatible(request_queue,iter)==true)
                        return true;
                    else{
                        txn->set_transaction_state(TransactionState::ABORTED);
                        return true;
                    }
                });
                if(txn->get_state()==TransactionState::ABORTED) 
                    throw TransactionAbortException (txn->get_txn_id(), AbortReason::DEAD_LOCK_PREVENT_NO_WAIT) ;
                iter->granted_ = true;
                return true; 
            }
            //如果事务想要申请更高级的锁, 则需要对锁进行升级, 首先检查是否有正在升级的锁, 如果有, 则返回升级冲突
            else if(request_queue->upgrading_ == true){
                throw TransactionAbortException (txn->get_txn_id(), AbortReason::UPGRADE_CONFLICT);
            }
            else{
                //准备升级
                request_queue->upgrading_ = true; 
                iter->lock_mode_ = return_lock_mode;
                iter->granted_ = false;
                request_queue->cv_.wait(queue_lock, [&request_queue, &iter, &txn]{
                    if(Lock_manager::enable_no_wait_==false)
                        return Lock_manager::checkQueueCompatible(request_queue, iter) || 
                            txn->get_state()==TransactionState::ABORTED;
                    // deadlock prevent
                    if(Lock_manager::checkQueueCompatible(request_queue,iter)==true)
                        return true;
                    else{
                        txn->set_transaction_state(TransactionState::ABORTED);
                        return true;
                    }
                });
                request_queue->upgrading_ = false;
                if(txn->get_state()==TransactionState::ABORTED) 
                    throw TransactionAbortException (txn->get_txn_id(), AbortReason::DEAD_LOCK_PREVENT_NO_WAIT) ;
                iter->granted_ = true;
                return true;
            } 
        }
    }
    return false;
}

auto Lock_manager::checkQueueCompatible(const LockRequestQueue *request_queue, const LockRequest *request) -> bool {
    for(auto iter : request_queue->request_queue_ ){
        if(iter != request && iter->granted_ == true){
            if(isLockCompatible(iter, request->lock_mode_) == false)
                return false;
        }
    }
    return true;
}

auto Lock_manager::LockTable(Transaction *txn, LockMode lock_mode, const table_oid_t &oid) -> bool {
    
    //步骤一: 检查事务状态
    if(txn->get_state() == TransactionState::DEFAULT){
        txn->set_transaction_state(TransactionState::GROWING);
    } 
    if(txn->get_state() != TransactionState::GROWING){
        throw TransactionAbortException (txn->get_txn_id(), AbortReason::LOCK_ON_SHRINKING);
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
    if(checkSameTxnLockRequest(txn, request_queue, target_lock_mode, queue_lock)==true){
        if(txn->get_state() == TransactionState::ABORTED)
            return false;
        return true;
    }
    
    //步骤五, 如果当前事务在请求队列中没有申请该数据项的锁, 则新建请求加入队列
    //检查是否可以上锁, 否则阻塞, 使用条件变量cv来实现
    LockRequest* lock_request = new LockRequest(txn->get_txn_id(), target_lock_mode); 
    request_queue->request_queue_.emplace_back(lock_request); 
    txn->get_lock_set()->emplace(l_id);

    request_queue->cv_.wait(queue_lock, [&request_queue, &lock_request, &txn]{
        // 如果不使用NO-Wait算法，则检查队列锁请求兼容情况和事务状态，
        // 如果可以锁请求兼容或事务已经回滚，则返回true，跳出等待
        if(Lock_manager::enable_no_wait_==false)
            return Lock_manager::checkQueueCompatible(request_queue, lock_request) || 
                txn->get_state()==TransactionState::ABORTED;
        // 如果使用No-Wait算法， 如果当前请求与锁请求队列兼容
        // 那么返回true，跳出等待
        if(Lock_manager::checkQueueCompatible(request_queue,lock_request)==true)
            return true;
        // 否则，当前事务回滚，跳出等待
        else{
            txn->set_transaction_state(TransactionState::ABORTED);
            return true;
        }
    });
    if(txn->get_state()==TransactionState::ABORTED) 
        throw TransactionAbortException (txn->get_txn_id(), AbortReason::DEAD_LOCK_PREVENT_NO_WAIT) ;
    lock_request->granted_ = true;

    return true;
}

auto Lock_manager::LockPartition(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id) -> bool {
    // for debug
    std::cout << oid << " " << p_id << std::endl;

    //检查事务状态
    if(txn->get_state() == TransactionState::DEFAULT){
        txn->set_transaction_state(TransactionState::GROWING);
    } 
    if(txn->get_state() != TransactionState::GROWING){
        throw TransactionAbortException (txn->get_txn_id(), AbortReason::LOCK_ON_SHRINKING);
    }

    Lock_data_id l_id(oid, p_id, Lock_data_type::PARTITION);
    Lock_data_id parent_table_l_id(oid, Lock_data_type::TABLE);

    // for benchmark debug
    // if(lock_mode == LockMode::SHARED || lock_mode == LockMode::INTENTION_SHARED){
    //     //检查父节点是否上IS锁
    //     if(txn->get_table_IS_lock_set()->count(parent_table_l_id)==0){
    //         throw TransactionAbortException(txn->get_txn_id(), AbortReason::TABLE_LOCK_NOT_PRESENT);
    //     }
    // } 
    // else if(lock_mode == LockMode::EXLUCSIVE || lock_mode == LockMode::INTENTION_EXCLUSIVE || lock_mode == LockMode::S_IX){
    //     //检查父节点是否上IX锁
    //     if(txn->get_table_IX_lock_set()->count(parent_table_l_id)==0){
    //         throw TransactionAbortException(txn->get_txn_id(), AbortReason::TABLE_LOCK_NOT_PRESENT);
    //     }
    // }
    
    //通过mutex申请全局锁表
    std::unique_lock<std::mutex> Latch(latch_);
    LockRequestQueue* request_queue = &lock_map_[l_id];
    std::unique_lock<std::mutex> queue_lock(request_queue->latch_);
    Latch.unlock();

    auto target_lock_mode = lock_mode;
    if(checkSameTxnLockRequest(txn, request_queue, target_lock_mode, queue_lock)==true){
        if(txn->get_state() == TransactionState::ABORTED)
            return false;
        return true;
    }

    //检查是否可以上锁, 否则阻塞, 使用条件变量cv来实现
    LockRequest* lock_request = new LockRequest(txn->get_txn_id(), lock_mode); 
    request_queue->request_queue_.emplace_back(lock_request); 
    txn->get_lock_set()->emplace(l_id);

    request_queue->cv_.wait(queue_lock, [&request_queue, &lock_request, &txn]{
                    //TODO deadlock
                    if(Lock_manager::enable_no_wait_==false)
                        return Lock_manager::checkQueueCompatible(request_queue, lock_request) || 
                            txn->get_state()==TransactionState::ABORTED;
                    if(Lock_manager::checkQueueCompatible(request_queue,lock_request)==true)
                        return true;
                    else{
                        txn->set_transaction_state(TransactionState::ABORTED);
                        return true;
                    }
                });
    if(txn->get_state()==TransactionState::ABORTED) 
        throw TransactionAbortException (txn->get_txn_id(), AbortReason::DEAD_LOCK_PREVENT_NO_WAIT) ;
        
    lock_request->granted_ = true;

    return true;
}

auto Lock_manager::LockRow(Transaction *txn, LockMode lock_mode, const table_oid_t &oid, const partition_id_t &p_id, const row_id_t &row_id) -> bool {

    if(txn->get_state() == TransactionState::DEFAULT){
        txn->set_transaction_state(TransactionState::GROWING);
    } 

    if(txn->get_state() != TransactionState::GROWING){
        // std :: cout << static_cast<int>(txn->get_state()) << "*********" << std::endl;
        throw TransactionAbortException (txn->get_txn_id(), AbortReason::LOCK_ON_SHRINKING);
    }
    if(lock_mode != LockMode::SHARED && lock_mode != LockMode::EXLUCSIVE){
        throw TransactionAbortException(txn->get_txn_id(), AbortReason::ATTEMPTED_INTENTION_LOCK_ON_ROW);
    }
    
    Lock_data_id l_id(oid, p_id, row_id, Lock_data_type::ROW);
    Lock_data_id parent_partition_l_id(oid, p_id, Lock_data_type::PARTITION);

    // 暂时注释，for benchmark
    // if(lock_mode == LockMode::SHARED || lock_mode == LockMode::INTENTION_SHARED){
    //     //检查父节点是否上IS锁
    //     if(txn->get_partition_IS_lock_set()->count(parent_partition_l_id)==0){
    //         throw TransactionAbortException(txn->get_txn_id(), AbortReason::PARTITION_LOCK_NOT_PRESENT);
    //     }
    // } 
    // else if(lock_mode == LockMode::EXLUCSIVE || lock_mode == LockMode::INTENTION_EXCLUSIVE || lock_mode == LockMode::S_IX){
    //     //检查父节点是否上IX锁
    //     if(txn->get_partition_IX_lock_set()->count(parent_partition_l_id)==0){
    //         throw TransactionAbortException(txn->get_txn_id(), AbortReason::PARTITION_LOCK_NOT_PRESENT);
    //     }
    // }

    //通过mutex申请全局锁表
    std::unique_lock<std::mutex> Latch(latch_);
    LockRequestQueue* request_queue = &lock_map_[l_id];
    std::unique_lock<std::mutex> queue_lock(request_queue->latch_);
    Latch.unlock();

    auto target_lock_mode = lock_mode;
    if(checkSameTxnLockRequest(txn, request_queue, target_lock_mode, queue_lock)==true){
        if(txn->get_state() == TransactionState::ABORTED)
            return false;
        return true;
    }

    LockRequest* lock_request = new LockRequest(txn->get_txn_id(), lock_mode); 
    request_queue->request_queue_.emplace_back(lock_request); 
    txn->get_lock_set()->emplace(l_id);

    request_queue->cv_.wait(queue_lock, [&request_queue, &lock_request, &txn]{
                    //TODO deadlock
                    if(Lock_manager::enable_no_wait_==false)
                        return Lock_manager::checkQueueCompatible(request_queue, lock_request) || 
                            txn->get_state()==TransactionState::ABORTED;
                    if(Lock_manager::checkQueueCompatible(request_queue,lock_request)==true)
                        return true;
                    else{
                        txn->set_transaction_state(TransactionState::ABORTED);
                        return true;
                    }
                });
    if(txn->get_state()==TransactionState::ABORTED) 
        throw TransactionAbortException (txn->get_txn_id(), AbortReason::DEAD_LOCK_PREVENT_NO_WAIT) ;

    lock_request->granted_ = true;
    
    return true;
}

auto Lock_manager::Unlock(Transaction *txn, const Lock_data_id &l_id) -> bool {

    std::unique_lock<std::mutex> Latch(latch_);
    LockRequestQueue* request_queue = &lock_map_[l_id];
    std::unique_lock<std::mutex> queue_lock(request_queue->latch_);
    Latch.unlock();
    
    if(txn->get_state() == TransactionState::GROWING){
        txn->set_transaction_state(TransactionState::SHRINKING);
    }

    auto iter = request_queue->request_queue_.begin();
    for( ; iter != request_queue->request_queue_.end(); iter++){
        if((*iter)->txn_id_ == txn->get_txn_id()){
            break;
        }
    }

    request_queue->request_queue_.erase(iter);
    request_queue->cv_.notify_all();
    return true;
}
