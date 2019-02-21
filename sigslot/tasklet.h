//
// Created by Dave Cridland on 2019-01-23.
//

#ifndef SIGSLOT_TASKLET_H
#define SIGSLOT_TASKLET_H

#include <sigslot/sigslot.h>
#include <experimental/coroutine>
#include <string>

namespace sigslot {
    template<typename T> struct tasklet;

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
                return coro.promise().get();
            }

            bool started() const {
                return coro.promise().started;
            }

            void start() {
                if (!coro) throw std::logic_error("No coroutine to start");
                if (coro.done()) throw std::logic_error("Already run");
                if (coro.promise().started) throw std::logic_error("Already started");
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

        template<typename T>
        struct awaitable {
            std::experimental::coroutine_handle<> awaiting = nullptr;
            sigslot::tasklet<T> const &task;

            awaitable(sigslot::tasklet<T> const &t) : task(t) {
                task.coro.promise().await_add(this);
            }

            awaitable(awaitable const &a) : task(a.task) {
                task.coro.promise().await_add(this);
            }

            awaitable(awaitable &&other) noexcept : task(other.task) {
                task.coro.promise().await_add(this);
            }

            bool await_ready() {
                if (!task.started()) {
                    // Need to start the task
                    const_cast<sigslot::tasklet<T> &>(task).start();
                }
                return !task.running();
            }

            void await_suspend(std::experimental::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
            }

            auto await_resume() {
                return task.get();
            }

            void resolve() {
                if (awaiting) awaiting.resume();
            }

            ~awaitable() {
                task.coro.promise().await_del(this);
            }
        };


        struct promise_type_base {
            std::string name;
            std::exception_ptr eptr;
            sigslot::signal<> complete;
            sigslot::signal<std::exception_ptr &> exception;
            bool started = false;

            void set_name(std::string const &s) {
                name = s;
            }

            auto final_suspend() {
                complete();
                return std::experimental::suspend_always{};
            }

            auto initial_suspend() {
                return std::experimental::suspend_always{};
            }

            void unhandled_exception() {
                eptr = std::current_exception();
                exception(eptr);
            }

            void get() const {
                if (eptr) {
                    std::rethrow_exception(eptr);
                }
            }

            virtual ~promise_type_base() {
                using namespace std::string_literals;
                name = "** Destroyed **"s;
            }
        };

        template<typename R, typename T>
        struct promise_type : public promise_type_base {
            T value;
            std::set<awaitable<T> *> awaiters;
            typedef std::experimental::coroutine_handle<promise_type<R, T>> handle_type;

            promise_type() : value() {}

            void await_add(awaitable<T> * a) {
                awaiters.insert(a);
            }

            void await_del(awaitable<T> * a) {
                awaiters.erase(a);
            }

            auto get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            auto return_value(T v) {
                value = v;
                return std::experimental::suspend_never{};
            }

            auto get() const {
                promise_type_base::get();
                return value;
            }
        };

        template<typename R>
        struct promise_type<R, void> : public promise_type_base {
            typedef std::experimental::coroutine_handle<promise_type<R, void>> handle_type;
            std::set<awaitable<void> *> awaiters;

            void await_add(awaitable<void> * a) {
                awaiters.insert(a);
            }

            void await_del(awaitable<void> * a) {
                awaiters.erase(a);
            }

            auto get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            auto return_void() {
                return std::experimental::suspend_never{};
            }
        };
    }


    template<typename T>
    struct tasklet : public internal::tasklet<std::experimental::coroutine_handle<internal::promise_type<tasklet<T>,T>>> {
        using promise_type = internal::promise_type<tasklet<T>,T>;
        tasklet() = delete;
    };

    template<typename T>
    auto operator co_await(tasklet<T> const &task) {
        return internal::awaitable<T>(task);
    }
}

#endif //SIGSLOT_TASKLET_H
