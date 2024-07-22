//
// Created by dave on 29/03/2024.
//

#include <sigslot/resume.h>

int resumptions = 0;

namespace sigslot {
    static inline void resume(std::coroutine_handle<> coro) {
        ++resumptions;
        coro.resume();
    }
}
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
}

TEST(Resume, Trivial) {
    EXPECT_EQ(resumptions, 0);
    auto coro = trivial_task(42);
    EXPECT_TRUE(coro.running());
    EXPECT_FALSE(coro.started());
    auto result = coro.get();
    EXPECT_FALSE(coro.running());
    EXPECT_TRUE(coro.started());
    EXPECT_EQ(result, 42);
    EXPECT_EQ(resumptions, 0);
    resumptions = 0;
}

TEST(Resume, Basic) {
    EXPECT_EQ(resumptions, 0);
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
    EXPECT_EQ(resumptions, 1);
    resumptions = 0;
}
