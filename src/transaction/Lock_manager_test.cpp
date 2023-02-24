#include "gtest/gtest.h"
#include "Lock_manager.h"

const std::string TEST_DB_NAME = "LockManagerTestDB";

class LockManagerTest : public ::testing::Test {
   public:
    std::unique_ptr<Lock_manager> lock_manager_;

   public:
    void SetUp() override {
        ::testing::Test::SetUp();
        lock_manager_ = std::make_unique<Lock_manager>();
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
        lock_manager_->UnLockTable(&txn0, oid);
        EXPECT_EQ(txn0.get_state(), TransactionState::SHRINKING);
    });

    std::thread t1([&] {
        printf("t1\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        Transaction txn1(0);
        bool res = lock_manager_->LockTable(&txn1, LockMode::SHARED, oid);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn1.get_state(), TransactionState::GROWING);
        lock_manager_->UnLockTable(&txn1, oid);
        EXPECT_EQ(txn1.get_state(), TransactionState::SHRINKING);
    });

    t0.join();
    t1.join();
}

// test shared lock on tuple under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest1_SHARED_TUPLE) {
    std::vector<row_id_t> row_vec;
    std::vector<Transaction *> txns;
    int num = 10;

    for(int i = 0; i < num; ++i) {
        row_vec.push_back(row_id_t(i));
        Transaction* t = new Transaction(i);
        txns.emplace_back(t);
        EXPECT_EQ(i, txns[i]->get_txn_id());
    }

    auto task = [&](int txn_id) {
        bool res;

        res = lock_manager_->LockTable(txns[txn_id], LockMode::INTENTION_SHARED, 0);
        EXPECT_TRUE(res);
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);

        res = lock_manager_->LockPartition(txns[txn_id], LockMode::INTENTION_SHARED, 0, 0);
        EXPECT_TRUE(res);
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);

        for(const row_id_t &row : row_vec) {
            res = lock_manager_->LockRow(txns[txn_id], LockMode::SHARED, 0, 0, row);
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::GROWING);
        }

        for(const row_id_t &row : row_vec) {
            res = lock_manager_->UnLockRow(txns[txn_id], 0, 0, row);
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);
        }
        res = lock_manager_->UnLockPartition(txns[txn_id], 0, 0);
        EXPECT_TRUE(res);
        EXPECT_EQ(txns[txn_id]->get_state(), TransactionState::SHRINKING);

        res = lock_manager_->UnLockTable(txns[txn_id], 0);
        EXPECT_TRUE(res);
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