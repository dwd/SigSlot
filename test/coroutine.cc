//
// Created by dave on 29/03/2024.
//

#include <gtest/gtest.h>
#include <sigslot/sigslot.h>
#include <sigslot/tasklet.h>

namespace {
    sigslot::tasklet<int> trivial_task(int i) {
        co_return i;
    }

    sigslot::tasklet<int> basic_task(sigslot::signal<int> &signal) {
        co_return co_await signal;
    }

    sigslot::tasklet<int> nested_task(int i) {
        co_return co_await trivial_task(i);
    }

    sigslot::tasklet<int> exception_task(int i) {
        if (i == 42) {
            // Have to do this conditionally with a co_return otherwise it's not a coroutine.
            throw std::runtime_error("Help");
        }
        co_return i;
    }
}

TEST(Tasklet, Trivial) {
    auto coro = trivial_task(42);
    EXPECT_TRUE(coro.running());
    EXPECT_FALSE(coro.started());
    auto result = coro.get();
    EXPECT_FALSE(coro.running());
    EXPECT_TRUE(coro.started());
    EXPECT_EQ(result, 42);
}

TEST(Tasklet, Basic) {
    sigslot::signal<int> signal;

    auto coro = basic_task(signal);
    EXPECT_TRUE(coro.running());
    EXPECT_FALSE(coro.started());
    coro.start();
    EXPECT_TRUE(coro.running());
    EXPECT_TRUE(coro.started());
    signal(42);
    auto result = coro.get();
    EXPECT_EQ(result, 42);
}

TEST(Tasklet, Nested) {
    auto coro = nested_task(42);
    EXPECT_TRUE(coro.running());
    EXPECT_FALSE(coro.started());
    auto result = coro.get();
    EXPECT_FALSE(coro.running());
    EXPECT_TRUE(coro.started());
    EXPECT_EQ(result, 42);
}


TEST(Tasklet, Throw) {
    auto coro = exception_task(42);
    EXPECT_TRUE(coro.running());
    EXPECT_FALSE(coro.started());
    EXPECT_THROW(
        auto result = coro.get(),
        std::runtime_error
    );
    EXPECT_FALSE(coro.running());
    EXPECT_TRUE(coro.started());
}

