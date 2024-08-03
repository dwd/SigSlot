//
// Created by Dave Cridland on 2019-01-23.
//

#ifndef SIGSLOT_TASKLET_H
#define SIGSLOT_TASKLET_H

#include <sigslot/sigslot.h>
#include <coroutine>
#include <string>
#include <stdexcept>

namespace sigslot {
    template<typename T> struct tasklet;

    struct tracker {
        virtual void terminate() const {}
        virtual void exception(std::exception_ptr eptr) const {}
        virtual ~tracker() {}
    };
    template<typename T, typename ...Args>
    requires std::is_base_of_v<tracker, T>
    std::shared_ptr<T> track(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    namespace internal {
        template<typename T> struct awaitable;

        template<typename handle_type>
        struct tasklet {
            handle_type coro;

            explicit tasklet() : coro(nullptr) {}

            tasklet(handle_type h) : coro(h) {}

            tasklet(tasklet &&other) noexcept : coro(other.coro) {
                other.coro = nullptr;
            }

            tasklet(tasklet const &other) : coro(other.coro) {}

            tasklet &operator=(tasklet &&other) noexcept {
                coro = other.coro;
                other.coro = nullptr;
                return *this;
            }

            tasklet &operator=(tasklet const &) = delete;

            ~tasklet() {
                if (coro) coro.destroy();
            }

            auto get() const {
                if (!coro.promise().started) {
                    // Never started, so start now.
                    const_cast<tasklet *>(this)->start();
                }
                if (!coro.promise().finished) {
                    throw std::runtime_error("Not finished yet");
                }
                return coro.promise().get();
            }

            bool started() const {
                return coro.promise().started;
            }

            void start() {
                if (!coro) throw std::logic_error("No coroutine to start");
                if (coro.done()) throw std::logic_error("Already run");
                if (coro.promise().started) throw std::logic_error("Already started");
                if (coro.promise().finished) throw std::logic_error("Already finished");
                coro.promise().started = true;
                coro.resume();
            }

            bool running() const {
                if (!coro) return false;
                return !coro.done();
            }

            sigslot::signal<> &complete() {
                return coro.promise().complete;
            }

            sigslot::signal<std::exception_ptr const &> &exception() {
                return coro.promise().exception;
            }

            void set_name(std::string const &s) {
                coro.promise().set_name(s);
            }

            auto operator*() const {
                return get();
            }
        };

        struct awaitable_base {
            std::coroutine_handle<> awaiting = nullptr;
            bool resolved = false;

            awaitable_base() = default;
            awaitable_base(awaitable_base const & other) = default;
            awaitable_base(awaitable_base && other) noexcept : awaiting(other.awaiting) {
                other.awaiting = nullptr;
            }
            void await_suspend(std::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
            }

            void resolve() {
                resolved = true;
                if (awaiting) ::sigslot::resume_switch(awaiting);
            }

            virtual ~awaitable_base() = default;
        };

        template<typename T>
        struct awaitable : public awaitable_base {
            sigslot::tasklet<T> const &task;

            awaitable(sigslot::tasklet<T> const &t) : awaitable_base(), task(t) {
                task.coro.promise().await_add(this);
            }

            awaitable(awaitable const &other) : awaitable_base(other), task(other.task) {
                task.coro.promise().await_add(this);
            }

            awaitable(awaitable &&other) noexcept : awaitable_base(other), task(other.task) {
                task.coro.promise().await_add(this);
            }

            bool await_ready() {
                if (!task.started()) {
                    // Need to start the task
                    const_cast<sigslot::tasklet<T> &>(task).start();
                }
                return !task.running();
            }

            auto await_resume() {
                resolved = true;
                return task.get();
            }

            ~awaitable() {
                if (!resolved) {
                    task.coro.promise().await_del(this);
                }
            }
        };


        struct promise_type_base {
            std::string name;
            std::exception_ptr eptr;
            sigslot::signal<> complete;
            sigslot::signal<std::exception_ptr &> exception;
            bool started = false;
            bool finished = false;
            std::set<awaitable_base *> awaiters;
            std::shared_ptr<tracker> track;

            promise_type_base() {}
            template<typename Tracker>
            promise_type_base(std::shared_ptr<Tracker> const & t) : track(t) {}

            void await_add(awaitable_base * a) {
                awaiters.insert(a);
            }

            void await_del(awaitable_base * a) {
                awaiters.erase(a);
            }

            void set_name(std::string const &s) {
                name = s;
            }

            auto final_suspend() noexcept {
                finished = true;
                complete();
                if (track) {
                    track->terminate();
                    track = nullptr;
                }
                auto all_awaiters = std::move(awaiters); // Move to keep it on the stack.
                for (auto awaiter : all_awaiters) {
                    awaiter->resolve();
                }
                return std::suspend_always{};
            }

            auto initial_suspend() {
                return std::suspend_always{};
            }

            void unhandled_exception() {
                eptr = std::current_exception();
                if (track) {
                    track->exception(eptr);
                    track = nullptr;
                }
                exception(eptr);
            }

            void throw_exception() const {
                if (eptr) {
                    std::rethrow_exception(eptr);
                }
            }

            virtual ~promise_type_base() {
                if (track) {
                    track->terminate();
                }
                using namespace std::string_literals;
                name = "** Destroyed **"s;
            }
        };

        template<typename R, typename T>
        struct promise_type : public promise_type_base {
            T value;
            typedef std::coroutine_handle<promise_type<R, T>> handle_type;

            promise_type() : value() {}

            template<typename Tracker, typename ...Args>
            requires std::is_base_of_v<tracker,Tracker>
            promise_type(std::shared_ptr<Tracker> const & t, Args&&...) : promise_type_base(t), value() {}

            auto get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            auto return_value(T v) {
                if (track) {
                    track->terminate();
                    track = nullptr;
                }
                value = v;
                return std::suspend_never{};
            }

            auto get() const {
                throw_exception();
                return value;
            }
        };

        template<typename R>
        struct promise_type<R, void> : public promise_type_base {
            typedef std::coroutine_handle<promise_type<R, void>> handle_type;

            template<typename Tracker, typename ...Args>
            requires std::is_base_of_v<tracker,Tracker>
            promise_type(std::shared_ptr<Tracker> const & t, Args&&...) : promise_type_base(t) {}

            promise_type() {}

            auto get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            auto return_void() {
                if (track) {
                    track->terminate();
                    track = nullptr;
                }
                return std::suspend_never{};
            }

            void get() const {
                throw_exception();
            }
        };
    }


    template<typename T>
    struct tasklet : public internal::tasklet<std::coroutine_handle<internal::promise_type<tasklet<T>,T>>> {
        using promise_type = internal::promise_type<tasklet<T>,T>;
        using value_type = T;
    };

    template<typename T>
    auto operator co_await(tasklet<T> const &task) {
        return internal::awaitable<T>(task);
    }
}

#endif //SIGSLOT_TASKLET_H
