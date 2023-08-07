#include "gtest/gtest.h"
#include "Lock_manager.h"

const std::string TEST_DB_NAME = "LockManagerTestDB";

class LockManagerTest : public ::testing::Test {
   public:
    std::unique_ptr<Lock_manager> lock_manager_;

   public:
    void SetUp() override {
        ::testing::Test::SetUp();
        lock_manager_ = std::make_unique<Lock_manager>(true);
    }
};

TEST_F(LockManagerTest, TransactionStateTest) {
    table_oid_t oid = 1;
    Lock_data_id lock_data_id(oid, Lock_data_type::TABLE);

    printf("before\n");
    std::thread t0([&] {
        printf("t0\n");
        Transaction txn0(0);
        bool res = lock_manager_->LockTable(&txn0, LockMode::SHARED, oid);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        lock_manager_->Unlock(&txn0, lock_data_id);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
    });

    std::thread t1([&] {
        printf("t1\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        Transaction txn1(0);
        bool res = lock_manager_->LockTable(&txn1, LockMode::SHARED, oid);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn1.get_state(), TransactionState::GROWING);
        lock_manager_->Unlock(&txn1, lock_data_id);
        EXPECT_EQ(txn1.get_state(), TransactionState::SHRINKING);
    });

    t0.join();
    t1.join();
}

// // test shared lock on tuple under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest1_SHARED_TABLE) {
    std::vector<row_id_t> row_vec;
    std::vector<Transaction *> txns;
    int num = 5;

    for(int i = 0; i < num; ++i) {
        row_vec.push_back(row_id_t(i));
        Transaction* t = new Transaction(i);
        txns.emplace_back(t);
        EXPECT_EQ(i, txns[i]->get_txn_id());
    }

    auto task = [&](int txn_id) {  
        Lock_data_id lock_table_id(0, Lock_data_type::TABLE);
        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);

        std::this_thread::sleep_for(std::chrono::milliseconds(rand()%500));

        bool res;

        res = lock_manager_->LockTable(txns[txn_id], LockMode::INTENTION_SHARED, 0);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << "LockTable IS o_id: 0";
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);

        res = lock_manager_->LockPartition(txns[txn_id], LockMode::INTENTION_SHARED, 0, 0);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << "LockPartition IS p_id: 0" << std::endl;
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);

        for(const row_id_t &row : row_vec) {
            res = lock_manager_->LockRow(txns[txn_id], LockMode::SHARED, 0, 0, row);
            std::this_thread::sleep_for(std::chrono::milliseconds(rand()%200));
            EXPECT_TRUE(res);
            std::cout << "txn_id : " << txn_id << "LockRow S row_id: (" << row << ")" << std::endl;
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);
        }

        for(const row_id_t &row : row_vec) {
            Lock_data_id lock_data_id(0, 0, row, Lock_data_type::ROW);
            res = lock_manager_->Unlock(txns[txn_id], lock_data_id);
            EXPECT_TRUE(res);
            std::cout << "txn_id : " << txn_id << "Unlock S row_id: (" << row << ")" << std::endl;
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);
        }
        res = lock_manager_->Unlock(txns[txn_id], lock_par_id);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << "Unlock p_id: 0";
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);

        res = lock_manager_->Unlock(txns[txn_id], lock_table_id);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << "Unlock o_id: 0" << std::endl;
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);

        //TODO
        // txn_manager_->Commit(txns[txn_id], log_manager_.get());
        // EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::COMMITTED);
    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }

    for(int i = 0; i < num; ++i) {
        delete txns[i];
    }
}


// test exclusive lock on tuple under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest2_EXCLUSIVE_TUPLE) {
    std::vector<row_id_t> row_vec;
    std::vector<Transaction *> txns;
    int num = 5;

    for(int i = 0; i < num; ++i) {
        row_vec.push_back(row_id_t(i));
        Transaction* t = new Transaction(i);
        txns.emplace_back(t);
        EXPECT_EQ(i, txns[i]->get_txn_id());
    }

    auto task = [&](int txn_id) {
        Lock_data_id lock_table_id(0, Lock_data_type::TABLE);
        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);

        std::this_thread::sleep_for(std::chrono::milliseconds(rand()%500));

        bool res;

        res = lock_manager_->LockTable(txns[txn_id], LockMode::INTENTION_EXCLUSIVE, 0);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << " LockTable IX o_id: 0";
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);

        res = lock_manager_->LockPartition(txns[txn_id], LockMode::INTENTION_EXCLUSIVE, 0, 0);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << " LockPartition IX p_id: 0" << std::endl;
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);

        for(const row_id_t &row : row_vec) {
            res = lock_manager_->LockRow(txns[txn_id], LockMode::EXLUCSIVE, 0, 0, row);
            EXPECT_TRUE(res);
            std::this_thread::sleep_for(std::chrono::milliseconds(rand()%200));
            std::cout << "txn_id : " << txn_id << " LockRow X row_id: (" << row << ")" << std::endl;
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);
        }

        for(const row_id_t &row : row_vec) {
            Lock_data_id lock_data_id(0, 0, row, Lock_data_type::ROW);
            res = lock_manager_->Unlock(txns[txn_id], lock_data_id);
            EXPECT_TRUE(res);
            std::this_thread::sleep_for(std::chrono::milliseconds(rand()%200));
            std::cout << "txn_id : " << txn_id << " Unlock X row_id: (" << row << ")" << std::endl;
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);
        }
        res = lock_manager_->Unlock(txns[txn_id], lock_par_id);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << "Unlock p_id: 0";
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);

        res = lock_manager_->Unlock(txns[txn_id], lock_table_id);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << "Unlock o_id: 0" << std::endl;
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);

        //TODO
        // txn_manager_->Commit(txns[txn_id], log_manager_.get());
        // EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::COMMITTED);
    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }

    for(int i = 0; i < num; ++i) {
        delete txns[i];
    }
}



// test exclusive lock on Partition under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest3_EXCLUSIVE_Partition) {
    std::vector<partition_id_t> p_vec;
    std::vector<Transaction *> txns;
    int num = 5;

    for(int i = 0; i < num; ++i) {
        p_vec.push_back(partition_id_t(i));
        Transaction* t = new Transaction(i);
        txns.emplace_back(t);
        EXPECT_EQ(i, txns[i]->get_txn_id());
    }

    auto task = [&](int txn_id) {
        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(rand()%500));

        bool res;

        res = lock_manager_->LockTable(txns[txn_id], LockMode::INTENTION_EXCLUSIVE, 0);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << " LockTable IX o_id: 0" << std::endl;
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);

        for(const partition_id_t &p : p_vec) {
            res = lock_manager_->LockPartition(txns[txn_id], LockMode::EXLUCSIVE, 0, p);
            EXPECT_TRUE(res);
            std::this_thread::sleep_for(std::chrono::milliseconds(rand()%200));
            std::cout << "txn_id : " << txn_id << " LockPartition X p_id: (" << p << ")" << std::endl;
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);
        }

        for(const row_id_t &p : p_vec) {
            Lock_data_id lock_data_id(0, p, Lock_data_type::PARTITION);
            res = lock_manager_->Unlock(txns[txn_id], lock_data_id);
            EXPECT_TRUE(res);
            std::this_thread::sleep_for(std::chrono::milliseconds(rand()%200));
            std::cout << "txn_id : " << txn_id << " Unlock X p_id: (" << p << ")" << std::endl;
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);
        }

        res = lock_manager_->Unlock(txns[txn_id], lock_tab_id);
        EXPECT_TRUE(res);
        std::cout << "txn_id : " << txn_id << "Unlock o_id: 0" << std::endl;
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);

        //TODO
        // txn_manager_->Commit(txns[txn_id], log_manager_.get());
        // EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::COMMITTED);
    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }

    for(int i = 0; i < num; ++i) {
        delete txns[i];
    }
}

// test intention shared lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest4_INTENTION_SHARED) {
    // txnA -> table1.tuple{1,1} shared
    // txnB -> table1 exclusive
    row_id_t row_id = 0;

    std::vector<int> operation;

    std::thread t0([&] {
        Transaction txn0(0);
        bool res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_SHARED, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);
        
        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_SHARED, 0, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockRow(&txn0, LockMode::SHARED, 0, 0, row_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        operation.push_back(0);
        
        Lock_data_id lock_data_id(0, 0, row_id, Lock_data_type::ROW);
        res = lock_manager_->Unlock(&txn0, lock_data_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);
        res = lock_manager_->Unlock(&txn0, lock_par_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        res = lock_manager_->Unlock(&txn0, lock_tab_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
    });

    std::thread t1([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds (500));

       Transaction txn1(1);
       bool res = lock_manager_->LockTable(&txn1, LockMode::EXLUCSIVE, 0);
       operation.push_back(1);
       EXPECT_EQ(res, true);
       EXPECT_EQ(txn1.get_state(), TransactionState::GROWING);

       Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
       res = lock_manager_->Unlock(&txn1, lock_tab_id);
       EXPECT_EQ(res, true);
       EXPECT_EQ(txn1.get_state(), TransactionState::SHRINKING);
    });

    t0.join();
    t1.join();

    // 如果txn1加锁没有被阻塞，那么一定是先执行operation.push_back(1)，反之，先执行operation.push_back(0)
    std::vector<int> operation_expected;
    operation_expected.push_back(0);
    operation_expected.push_back(1);
    EXPECT_EQ(operation_expected, operation);

}

// test intention shared lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest5_INTENTION_EXLUCSIVE) {
    // txnA -> table1.tuple{1,1} shared
    // txnB -> table1 exclusive
    row_id_t row_id = 0;

    std::vector<int> operation;

    std::thread t0([&] {
        Transaction txn0(0);
        bool res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_EXCLUSIVE, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);
        
        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_EXCLUSIVE, 0, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockRow(&txn0, LockMode::EXLUCSIVE, 0, 0, row_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        operation.push_back(0);

        Lock_data_id lock_data_id(0, 0, row_id, Lock_data_type::ROW);
        res = lock_manager_->Unlock(&txn0, lock_data_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);
        res = lock_manager_->Unlock(&txn0, lock_par_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
        
        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        res = lock_manager_->Unlock(&txn0, lock_tab_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
    });

    std::thread t1([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds (500));

        Transaction txn1(1);
        bool res = lock_manager_->LockTable(&txn1, LockMode::SHARED, 0);
        operation.push_back(1);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn1.get_state(), TransactionState::GROWING);

        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        res = lock_manager_->Unlock(&txn1, lock_tab_id);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn1.get_state(), TransactionState::SHRINKING);
    });

    t0.join();
    t1.join();

    // 如果txn1加锁没有被阻塞，那么一定是先执行operation.push_back(1)，反之，先执行operation.push_back(0)
    std::vector<int> operation_expected;
    operation_expected.push_back(0);
    operation_expected.push_back(1);
    EXPECT_EQ(operation_expected, operation);

}

TEST_F(LockManagerTest, Test_Lock_UnLock_Lock){

    std::thread t0([&] {
        printf("t0\n");
        Transaction txn0(0);
        bool res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_SHARED, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        lock_manager_->Unlock(&txn0, lock_tab_id);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_SHARED, 0);
        EXPECT_EQ(res, false);
        EXPECT_EQ(txn0.get_state(), TransactionState::ABORTED);

    });

    t0.join();
}

TEST_F(LockManagerTest, UpgradeTest1_upgrade_SToX){

    std::thread t0([&] {
        Transaction txn0(0);
        bool res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_SHARED, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_SHARED, 0, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockRow(&txn0, LockMode::SHARED, 0, 0, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_EXCLUSIVE, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_EXCLUSIVE, 0, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockRow(&txn0, LockMode::EXLUCSIVE, 0, 0, 0);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        Lock_data_id lock_row_id(0, 0, 0, Lock_data_type::ROW);
        lock_manager_->Unlock(&txn0, lock_row_id);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);
        lock_manager_->Unlock(&txn0, lock_par_id);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        lock_manager_->Unlock(&txn0, lock_tab_id);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

    });

    t0.join();

}

TEST_F(LockManagerTest, UpgradeTest2_muti_upgrade){

    int num = 5;
    
    auto task = [&](txn_id_t txn_id){
        Transaction txn0(txn_id);
        bool res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_SHARED, 0);
        std::cout << txn_id << " : LockTable IS res: " << res << std::endl; 
        // EXPECT_EQ(res, true);
        // EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_SHARED, 0, 0);
        std::cout << txn_id << " : LockPartition IS res: " << res << std::endl; 
        // EXPECT_EQ(res, true);
        // EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockRow(&txn0, LockMode::SHARED, 0, 0, 0);
        std::cout << txn_id << " : LockRow S res: " << res << std::endl; 
        // EXPECT_EQ(res, true);
        // EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_EXCLUSIVE, 0);
        std::cout << txn_id << " : LockTable IX res: " << res << std::endl; 
        // EXPECT_EQ(res, true);
        // EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_EXCLUSIVE, 0, 0);
        std::cout << txn_id << " : LockPartition IX res: " << res << std::endl; 
        // EXPECT_EQ(res, true);
        // EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        res = lock_manager_->LockRow(&txn0, LockMode::EXLUCSIVE, 0, 0, 0);
        std::cout << txn_id << " : LockRow X res: " << res << std::endl; 
        // EXPECT_EQ(res, true);
        // EXPECT_EQ(txn0.get_state(), TransactionState::GROWING);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        Lock_data_id lock_row_id(0, 0, 0, Lock_data_type::ROW);
        lock_manager_->Unlock(&txn0, lock_row_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);
        lock_manager_->Unlock(&txn0, lock_par_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
        
        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        lock_manager_->Unlock(&txn0, lock_tab_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }


}
TEST_F(LockManagerTest, NO_WAIT_TEST){

}

TEST_F(LockManagerTest, DeadLock_Test1){
    
    std::thread t0([&]{
        txn_id_t txn_id = 0;
        Transaction txn0(txn_id);
        bool res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_SHARED, 0);
        std::cout << txn_id << " : LockTable IS res: " << res << std::endl; 

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_SHARED, 0, 0);
        std::cout << txn_id << " : LockPartition IS res: " << res << std::endl; 

        res = lock_manager_->LockRow(&txn0, LockMode::SHARED, 0, 0, 0);
        std::cout << txn_id << " : LockRow 0 S res: " << res << std::endl; 

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_EXCLUSIVE, 0);
        std::cout << txn_id << " : LockTable IX res: " << res << std::endl; 

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_EXCLUSIVE, 0, 0);
        std::cout << txn_id << " : LockPartition IX res: " << res << std::endl; 

        res = lock_manager_->LockRow(&txn0, LockMode::EXLUCSIVE, 0, 0, 1);
        std::cout << txn_id << " : LockRow 1 X res: " << res << std::endl; 

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        Lock_data_id lock_row_id(0, 0, 0, Lock_data_type::ROW);
        lock_manager_->Unlock(&txn0, lock_row_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        lock_row_id.row_id_ = 1;
        lock_manager_->Unlock(&txn0, lock_row_id);
        std::cout << txn_id << " : Unlock 1 res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);
        lock_manager_->Unlock(&txn0, lock_par_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
        
        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        lock_manager_->Unlock(&txn0, lock_tab_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
    
    });

    std::thread t1([&]{
        txn_id_t txn_id = 1;
        Transaction txn0(txn_id);
        bool res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_SHARED, 0);
        std::cout << txn_id << " : LockTable IS res: " << res << std::endl; 

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_SHARED, 0, 0);
        std::cout << txn_id << " : LockPartition IS res: " << res << std::endl; 

        res = lock_manager_->LockRow(&txn0, LockMode::SHARED, 0, 0, 1);
        std::cout << txn_id << " : LockRow 0 S res: " << res << std::endl; 

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        res = lock_manager_->LockTable(&txn0, LockMode::INTENTION_EXCLUSIVE, 0);
        std::cout << txn_id << " : LockTable IX res: " << res << std::endl; 

        res = lock_manager_->LockPartition(&txn0, LockMode::INTENTION_EXCLUSIVE, 0, 0);
        std::cout << txn_id << " : LockPartition IX res: " << res << std::endl; 

        res = lock_manager_->LockRow(&txn0, LockMode::EXLUCSIVE, 0, 0, 0);
        std::cout << txn_id << " : LockRow 1 X res: " << res << std::endl; 

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        Lock_data_id lock_row_id(0, 0, 0, Lock_data_type::ROW);
        lock_manager_->Unlock(&txn0, lock_row_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        lock_row_id.row_id_ = 1;
        lock_manager_->Unlock(&txn0, lock_row_id);
        std::cout << txn_id << " : Unlock 1 res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

        Lock_data_id lock_par_id(0, 0, Lock_data_type::PARTITION);
        lock_manager_->Unlock(&txn0, lock_par_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
        
        Lock_data_id lock_tab_id(0, Lock_data_type::TABLE);
        lock_manager_->Unlock(&txn0, lock_tab_id);
        std::cout << txn_id << " : Unlock res: " << res << std::endl; 
        // EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);

    });

    t0.join();
    t1.join();
}
