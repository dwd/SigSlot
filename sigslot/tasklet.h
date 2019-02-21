//
// Created by Dave Cridland on 2019-01-23.
//

#ifndef SIGSLOT_TASKLET_H
#define SIGSLOT_TASKLET_H

#include <sigslot/sigslot.h>
#include <experimental/coroutine>

namespace sigslot {
    namespace internal {

        template<typename handle_type>
        struct tasklet {
            handle_type coro;

            explicit tasklet() : coro(nullptr) {}

            tasklet(handle_type h) : coro(h) {}

            tasklet(tasklet &&other) : coro(other.coro) {
                other.coro = nullptr;
            }

            tasklet(tasklet const &other) : coro(other.coro) {}

            tasklet &operator=(tasklet &&other) {
                coro = other.coro;
                other.coro = nullptr;
                return *this;
            }

            tasklet &operator=(tasklet const &) = delete;

            ~tasklet() {
                if (coro) coro.destroy();
            }

            auto get() const {
                return coro.promise().get();
            }

            void start() {
                if (!coro) throw std::logic_error("No coroutine to start");
                if (coro.done()) throw std::logic_error("Already run");
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


        struct promise_type_base {
            std::string name;
            std::exception_ptr eptr;
            sigslot::signal<> complete;
            sigslot::signal<std::exception_ptr &> exception;

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

            ~promise_type_base() {
                using namespace std::string_literals;
                name = "** Destroyed **"s;
            }
        };

        template<typename R, typename T>
        struct promise_type : public promise_type_base {
            std::string name;
            T value;
            std::exception_ptr eptr;
            sigslot::signal<> complete;
            sigslot::signal<std::exception_ptr &> exception;
            typedef std::experimental::coroutine_handle<promise_type<R, T>> handle_type;

            promise_type() : value() {}

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
            std::string name;
            std::exception_ptr eptr;
            sigslot::signal<> complete;
            sigslot::signal<std::exception_ptr &> exception;
            typedef std::experimental::coroutine_handle<promise_type<R, void>> handle_type;

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
        T operator*() const {
            return internal::tasklet<std::experimental::coroutine_handle<internal::promise_type<tasklet<T>,T>>>::get();
        }
    };

    template<>
    struct tasklet<void> : public internal::tasklet<std::experimental::coroutine_handle<internal::promise_type<tasklet<void>,void>>> {
        using promise_type = internal::promise_type<tasklet<void>,void>;
    };

    template<typename T>
    auto operator co_await(tasklet<T> const &task) {
        struct awaitable : public has_slots {
            std::experimental::coroutine_handle<> awaiting = nullptr;
            tasklet<T> const &task;

            explicit awaitable(tasklet<T> const &t) : task(t) {
                task.coro.promise().complete.connect(this, &awaitable::resolve);
            }

            awaitable(awaitable const &a) : task(a.task) {
                task.coro.promise().complete.connect(this, &awaitable::resolve);
            }

            awaitable(awaitable &&other) noexcept : task(other.task) {
                task.coro.promise().complete.connect(this, &awaitable::resolve);
            }

            bool await_ready() {
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
            }
        };
        return awaitable(task);
    }
}

#endif //SIGSLOT_TASKLET_H
