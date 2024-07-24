//
// Created by dwd on 21/12/2021.
//

#ifndef SIGSLOT_COTHREAD_H
#define SIGSLOT_COTHREAD_H

#include <thread>
#include "sigslot/sigslot.h"
#include "sigslot/tasklet.h"

namespace sigslot {
    namespace cothread_internal {
        template<typename Result>
        struct awaitable {
            std::coroutine_handle<> awaiting = nullptr;

            awaitable() = default;
            awaitable(awaitable && other) = delete;
            awaitable(awaitable const &) = delete;

            bool await_ready() {
                return has_payload();
            }

            void await_suspend(std::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
                await();
            }

            auto await_resume() {
                return payload();
            }

            void resolve() {
                std::coroutine_handle<> a = nullptr;
                std::swap(a, awaiting);
                if (a) sigslot::resume_switch(a);
            }

            template<typename Fn, typename ...Args>
            void run(Fn & fn, Args&&... args) {
                auto wrapped_fn = [this, &fn](Args... a) {
                    try {
                        auto result = fn(a...);
                        {
                            std::lock_guard l_(m_mutex);
                            m_payload.emplace(result);
                            resolve();
                        }
                    } catch(...) {
                        std::lock_guard l_(m_mutex);
                        m_eptr = std::current_exception();
                        resolve();
                    }
                };
                m_thread.emplace(wrapped_fn, args...);
            }

            void check_await() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
            }

            bool has_payload() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
                std::lock_guard l_(m_mutex);
                return m_eptr || m_payload.has_value();
            }

            auto payload() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
                m_thread->join();
                m_thread.reset();
                if (m_eptr) std::rethrow_exception(m_eptr);
                return *m_payload;
            }

            void await() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
                std::lock_guard l_(m_mutex);
                if (m_eptr || m_payload.has_value()) {
                    resolve();
                }
            }

        private:
            std::optional<std::jthread> m_thread;
            std::optional<Result> m_payload;
            std::recursive_mutex m_mutex;
            std::exception_ptr m_eptr;
        };
        template<>
        struct awaitable<void> {
            std::coroutine_handle<> awaiting = nullptr;

            awaitable() = default;
            awaitable(awaitable && other) = delete;
            awaitable(awaitable const &) = delete;

            bool await_ready() {
                return is_done();
            }

            void await_suspend(std::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
                await();
            }

            void await_resume() {
                done();
            }

            void resolve() {
                std::coroutine_handle<> a = nullptr;
                std::swap(a, awaiting);
                if (a) sigslot::resume_switch(a);
            }

            template<typename Fn, typename ...Args>
            void run(Fn & fn, Args&&... args) {
                auto wrapped_fn = [this, &fn](Args... a) {
                    try {
                        fn(a...);
                        {
                            std::lock_guard l_(m_mutex);
                            m_done = true;
                            resolve();
                        }
                    } catch(...) {
                        std::lock_guard l_(m_mutex);
                        m_eptr = std::current_exception();
                        resolve();
                    }
                };
                m_thread.emplace(wrapped_fn, args...);
            }

            void check_await() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
            }

            bool is_done() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
                std::lock_guard l_(m_mutex);
                return m_eptr || m_done;
            }

            void done() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
                m_thread->join();
                m_thread.reset();
                if (m_eptr) std::rethrow_exception(m_eptr);
            }

            void await() {
                if (!m_thread.has_value()) throw std::logic_error("No thread started");
                std::lock_guard l_(m_mutex);
                if (m_eptr || m_done) {
                    resolve();
                }
            }

        private:
            std::optional<std::jthread> m_thread;
            bool m_done = false;
            std::exception_ptr m_eptr;
            std::recursive_mutex m_mutex;
        };
        template<typename T>
        struct awaitable_ptr {
            std::unique_ptr<awaitable<T>> m_guts;

            awaitable_ptr() : m_guts(std::make_unique<awaitable<T>>()) {}
            awaitable_ptr(awaitable_ptr &&) = default;

            awaitable<T> & operator co_await() {
                m_guts->check_await();
                return *m_guts;
            }
        };
    }

    template<typename Callable>
    class co_thread {
    public:
    private:
        Callable m_fn;
    public:

        template<typename ...Args>
        [[nodiscard]] auto operator() (Args && ...args) {
            cothread_internal::awaitable_ptr<decltype(m_fn(args...))> awaitable;
            awaitable.m_guts->run(m_fn, args...);
            return std::move(awaitable);
        }

        explicit co_thread(Callable && fn) : m_fn(std::move(fn)) {}
    };
}

#endif
