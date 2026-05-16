#include "error_system/memory/object_pool.h"
#include <gtest/gtest.h>

namespace error_system::memory {

    struct test_object_t {
        int value_ = 0;
        test_object_t() = default;
        explicit test_object_t(int val) : value_(val) {}
    };

    class object_pool_test : public ::testing::Test {};

    TEST_F(object_pool_test, instance_returns_singleton) {
        auto& pool1 = object_pool_t<test_object_t, 10>::instance();
        auto& pool2 = object_pool_t<test_object_t, 10>::instance();
        EXPECT_EQ(&pool1, &pool2);
    }

    TEST_F(object_pool_test, initial_state_is_empty) {
        auto& pool = object_pool_t<test_object_t, 10>::instance();
        EXPECT_TRUE(pool.empty());
        EXPECT_EQ(pool.size(), 0);
        EXPECT_EQ(pool.capacity(), 10);
    }

    TEST_F(object_pool_test, acquire_returns_valid_object) {
        auto& pool = object_pool_t<test_object_t, 10>::instance();
        auto* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(pool.size(), 1);
        EXPECT_FALSE(pool.empty());
    }

    TEST_F(object_pool_test, release_returns_object_to_pool) {
        auto& pool = object_pool_t<test_object_t, 10>::instance();
        auto* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);

        pool.release(obj);
        EXPECT_EQ(pool.size(), 0);
        EXPECT_TRUE(pool.empty());
        EXPECT_EQ(obj, nullptr);
    }

    TEST_F(object_pool_test, full_pool_returns_nullptr) {
        auto& pool = object_pool_t<test_object_t, 3>::instance();

        auto* obj1 = pool.acquire();
        auto* obj2 = pool.acquire();
        auto* obj3 = pool.acquire();
        auto* obj4 = pool.acquire();

        EXPECT_NE(obj1, nullptr);
        EXPECT_NE(obj2, nullptr);
        EXPECT_NE(obj3, nullptr);
        EXPECT_EQ(obj4, nullptr);
        EXPECT_TRUE(pool.full());
    }

    TEST_F(object_pool_test, acquire_after_release_works) {
        auto& pool = object_pool_t<test_object_t, 2>::instance();

        auto* obj1 = pool.acquire();
        ASSERT_NE(obj1, nullptr);

        pool.release(obj1);
        auto* obj2 = pool.acquire();

        EXPECT_NE(obj2, nullptr);
    }

    TEST_F(object_pool_test, multiple_acquire_release_cycles) {
        auto& pool = object_pool_t<test_object_t, 5>::instance();

        for (int i = 0; i < 100; ++i) {
            auto* obj = pool.acquire();
            ASSERT_NE(obj, nullptr);
            obj->value_ = i;
            pool.release(obj);
        }

        EXPECT_EQ(pool.size(), 0);
    }

    TEST_F(object_pool_test, instance_thread_local_returns_different_instance) {
        auto& global_pool = object_pool_t<test_object_t, 5>::instance();
        auto& thread_pool = object_pool_t<test_object_t, 5>::instance_thread_local();

        EXPECT_NE(&global_pool, &thread_pool);
    }

    TEST_F(object_pool_test, release_invalid_pointer_does_nothing) {
        auto& pool = object_pool_t<test_object_t, 5>::instance();

        test_object_t invalid_obj;
        test_object_t* ptr = &invalid_obj;

        pool.release(ptr);
        EXPECT_NE(ptr, nullptr);
    }

    TEST_F(object_pool_test, capacity_matches_template_parameter) {
        auto& pool = object_pool_t<test_object_t, 42>::instance();
        EXPECT_EQ(pool.capacity(), 42);
    }

}  // namespace error_system::memory
