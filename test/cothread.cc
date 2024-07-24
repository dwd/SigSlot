#include "gtest/gtest.h"
#include <iostream>
#include <regex>
#include <coroutine>
#include <list>
#include <sigslot/resume.h>
// Tiny event loop. co_thread won't work properly without,
// since it's got to (essentially) block while the thread runs.

std::mutex lock_me;
std::vector<std::coroutine_handle<>> resume_me;


namespace sigslot {
    void resume(std::coroutine_handle<> coro) {
        std::lock_guard l(lock_me);
        resume_me.push_back(coro);
    }
}

#include <sigslot/sigslot.h>
#include <sigslot/tasklet.h>
#include <sigslot/cothread.h>

template<typename R>
void run_until_complete_low(sigslot::tasklet<R> & coro) {
    if (!coro.started()) coro.start();
    while (coro.running()) {
        std::vector<std::coroutine_handle<>> current;
        {
            std::lock_guard l(lock_me);
            current.swap(resume_me);
        }
        std::cout << "Resuming " << current.size() << " coroutines." << std::endl;
        for (auto coro : current) {
            coro.resume();
        }
        current.clear();
        sleep(1);
        std::cout << "... tick" << std::endl;
    }
}
template<typename R>
R run_until_complete(sigslot::tasklet<R> & coro) {
    run_until_complete_low(coro);
    return coro.get();
}
template<>
void run_until_complete<void>(sigslot::tasklet<void> & coro) {
    run_until_complete_low(coro);
    coro.get();
}


sigslot::tasklet<bool> inner(std::string const & s) {
    std::cout << "Here!" << std::endl;
    sigslot::co_thread thread1([](std::string const &s) {
        std::cout << "There 1! " << s << std::endl;
        return true;
    });
    sigslot::co_thread thread2([]() {
        std::cout << "+ Launch" << std::endl;
        sleep(1);
        std::cout << "+ There 2!" << std::endl;
        sleep(1);
        std::cout << "+ End" << std::endl;
        return true;
    });
    std::cout << "Still here!" << std::endl;
    auto thread2_await = thread2();
    auto result1 = co_await thread1(s);
    std::cout << "Got result1:" << result1 << std::endl;
    auto result2 = co_await thread2_await;
    std::cout << "Got result2:" << result2 << std::endl;
    co_return true;
}

sigslot::tasklet<void> start() {
    std::string s = "Hello world!";
    auto result = co_await inner(s);
    std::cout << "Completed test with result " << result << std::endl;
}

namespace {
    sigslot::tasklet<int> trivial_task(int i) {
        co_return i;
    }

    sigslot::tasklet<int> basic_task(sigslot::signal<int> &signal) {
        co_return co_await signal;
    }

    sigslot::tasklet<int> signal_thread_task() {
        sigslot::signal<int> signal;
        sigslot::co_thread thread([&signal]() {
           sleep(1);
           signal(42);
           sleep(1);
           return 42;
        });
        auto thread_result = thread();
        auto result = co_await signal;
        co_await thread_result;
        co_return result;
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

    sigslot::tasklet<void> thread_exception_task() {
        sigslot::co_thread t([]{throw std::runtime_error("Potato!");});
        co_await t();
    }
}

TEST(CoThreadTest, CheckLoop) {
    auto coro = trivial_task(42);
    auto result = run_until_complete(coro);
    EXPECT_EQ(result, 42);
}

TEST(CoThreadTest, CheckLoop2) {
    sigslot::signal<int> signal;
    auto coro = basic_task(signal);
    coro.start();
    int i = 0;
    while (coro.running()) {
        std::vector<std::coroutine_handle<>> current;
        {
            std::lock_guard l(lock_me);
            current.swap(resume_me);
        }
        std::cout << "Resuming " << current.size() << " coroutines." << std::endl;
        for (auto coro : current) {
            coro.resume();
        }
        current.clear();
        sleep(1);
        if (i == 2) {
            std::cout << "Signalling" << std::endl;
            signal(42);
        }
        ++i;
        std::cout << "... tick" << std::endl;
    }
    auto result = coro.get();
    std::cout << "Result: " << result << std::endl;
}

TEST(CoThreadTest, Tests) {
    std::cout << "Start" << std::endl;
    auto coro = start();
    run_until_complete(coro);
    std::cout << "*** END ***" << std::endl;
}

TEST(CoThreadTest, Exception) {
    std::cout << "Start" << std::endl;
    auto coro = thread_exception_task();
    EXPECT_THROW(
    run_until_complete(coro),
    std::runtime_error
    );
    std::cout << "*** END ***" << std::endl;
}
