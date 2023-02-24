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

