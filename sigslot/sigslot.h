// sigslot.h: Signal/Slot classes
//
// Written by Sarah Thompson (sarah@telergy.com) 2002.
// Mangled by Dave Cridland <dave@cridland.net>, most recently in 2019.
//
// License: Public domain. You are free to use this code however you like, with the proviso that
//          the author takes on no responsibility or liability for any use.
//
// QUICK DOCUMENTATION
//
//              (see also the full documentation at http://sigslot.sourceforge.net/)
//
//      #define switches
//          SIGSLOT_NO_COROUTINES:
//          If not defined, this will provide an operator co_await(), so that coroutines can
//          co_await on a signal instead of registering a callback.
//
//      PLATFORM NOTES
//
//      The header file requires C++11 (certainly), C++14 (probably), and C++17 (maybe).
//      Coroutine support isn't well-tested, and might only work on CLang for now.
//
//      THREADING MODES
//
//       Only C++11 threading remains.
//
//      USING THE LIBRARY
//
//          See the full documentation at http://sigslot.sourceforge.net/
//
//

#ifndef SIGSLOT_H__
#define SIGSLOT_H__

#include <set>
#include <list>
#include <functional>
#include <mutex>
#ifndef SIGSLOT_NO_COROUTINES
#include <optional>
#include <coroutine>
#include <vector>
#endif

#include <sigslot/resume.h>

namespace sigslot {
#ifndef SIGSLOT_NO_COROUTINES
    template<typename R>
    inline void resume_dispatch(std::coroutine_handle<> coro) {
        resume(coro);
    }
    template<>
    inline void resume_dispatch<coroutines::sentinel>(std::coroutine_handle<> coro) {
        coro.resume();
    }
    inline void resume_switch(std::coroutine_handle<>  coro) {
        using return_type = decltype(resume(coro));
        resume_dispatch<return_type>(coro);
    }
    template<typename R>
    inline void register_dispatch(std::coroutine_handle<> coro) {
        register_coro(coro);
    }
    template<>
    inline void register_dispatch<coroutines::sentinel>(std::coroutine_handle<>) {}
    inline void register_switch(std::coroutine_handle<>  coro) {
        using return_type = decltype(register_coro(coro));
        register_dispatch<return_type>(coro);
    }
    template<typename R>
    inline void deregister_dispatch(std::coroutine_handle<> coro) {
        register_coro(coro);
    }
    template<>
    inline void deregister_dispatch<coroutines::sentinel>(std::coroutine_handle<>) {}
    inline void deregister_switch(std::coroutine_handle<>  coro) {
        using return_type = decltype(deregister_coro(coro));
        deregister_dispatch<return_type>(coro);
    }
#endif

    class has_slots;

    namespace internal {
        class _signal_base_lo {
        protected:
            std::recursive_mutex m_barrier;
        public:
            virtual void slot_disconnect(has_slots *pslot) = 0;
            virtual ~_signal_base_lo() = default;
        };
    }


    class has_slots
    {
    private:
        std::recursive_mutex m_barrier;

    public:
        has_slots() = default;

        has_slots(const has_slots& hs) = delete;
        has_slots(has_slots && hs) = delete;

        void signal_connect(internal::_signal_base_lo* sender)
        {
            std::scoped_lock lock(m_barrier);
            m_senders.insert(sender);
        }

        void signal_disconnect(internal::_signal_base_lo* sender)
        {
            std::scoped_lock lock(m_barrier);
            m_senders.erase(sender);
        }

        virtual ~has_slots()
        {
            disconnect_all();
        }

        void disconnect_all()
        {
            std::scoped_lock lock(m_barrier);
            for (auto i : m_senders) {
                i->slot_disconnect(this);
            }

            m_senders.erase(m_senders.begin(), m_senders.end());
        }

    private:
        std::set<internal::_signal_base_lo *>  m_senders;
    };

    namespace internal {
        template<class... args>
        class _connection
        {
        public:
            _connection(has_slots *pobject, std::function<void(args... a)> fn, bool once)
                    : one_shot(once), m_pobject(pobject), m_fn(fn) {}

            void emit(args... a)
            {
                m_fn(a...);
            }

            [[nodiscard]] has_slots* getdest() const
            {
                return m_pobject;
            }

            const bool one_shot = false;
            bool expired = false;
        private:
            has_slots* m_pobject;
            std::function<void(args...)> m_fn;
        };

        template<class... args>
        class _signal_base : public _signal_base_lo
        {
        public:
            _signal_base() = default;

            _signal_base(const _signal_base& s)
                    : _signal_base_lo(s), m_connected_slots()
            {
                std::scoped_lock lock(m_barrier);
                for (auto i : s.m_connected_slots) {
                    i->getdest()->signal_connect(this);
                    m_connected_slots.push_back(i->clone());
                }
            }

            _signal_base(_signal_base &&) = delete;

            virtual ~_signal_base()
            {
                disconnect_all();
            }

            void disconnect_all()
            {
                std::scoped_lock lock(m_barrier);
                for (auto i : m_connected_slots) {
                    i->getdest()->signal_disconnect(this);
                    delete i;
                }
                m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
            }

            void disconnect(has_slots* pclass)
            {
                std::scoped_lock lock(m_barrier);
                bool found{false};
                m_connected_slots.remove_if([pclass, &found](_connection<args...> * x) {
                    if (x->getdest() == pclass) {
                        delete x;
                        found = true;
                        return true;
                    }
                    return false;
                });
                if (found) pclass->signal_disconnect(this);
            }

            void slot_disconnect(has_slots* pslot) final
            {
                std::scoped_lock lock(m_barrier);
                m_connected_slots.remove_if(
                    [pslot](_connection<args...> * x) {
                        if (x->getdest() == pslot) {
                            delete x;
                            return true;
                        }
                        return false;
                    }
                );
            }

        protected:
            std::list<_connection<args...> *>  m_connected_slots;
        };

    }


#ifndef SIGSLOT_NO_COROUTINES
    namespace coroutines {
        template<class... args> struct awaitable;
    }
#endif


    template<class... args>
    class signal : public internal::_signal_base<args...>
    {
    public:
        signal() = default;

        signal(const signal<args...>& s) = default;

        void connect(has_slots *pclass, std::function<void(args...)> &&fn, bool one_shot = false)
        {
            std::scoped_lock lock{internal::_signal_base<args...>::m_barrier};
            auto *conn = new internal::_connection<args...>(
                    pclass, std::move(fn), one_shot);
            this->m_connected_slots.push_back(conn);
            pclass->signal_connect(this);
        }
        
        // Helper for ptr-to-member; call the member function "normally".
        template<class desttype>
        requires std::derived_from<desttype, has_slots>
        void connect(desttype *pclass, void (desttype::* memfn)(args...), bool one_shot = false)
        {
            this->connect(pclass, [pclass, memfn](args... a) { (pclass->*memfn)(a...); }, one_shot);
        }

        [[nodiscard]] std::unique_ptr<has_slots> connect(std::function<void(args...)> && fn, bool one_shot=false)
        {
            auto raii = std::make_unique<has_slots>();
            this->connect(raii.get(), std::move(fn), one_shot);
            return raii;
        }

        // This code uses the long-hand because it assumes it may mutate the list.
        void emit(args... a)
        {
            std::scoped_lock lock{internal::_signal_base<args...>::m_barrier};
            auto it = this->m_connected_slots.begin();
            auto itNext = it;
            auto itEnd = this->m_connected_slots.end();

            while(it != itEnd)
            {
                itNext = it;
                ++itNext;

                if ((*it)->one_shot) {
                    (*it)->expired = true;
                }
                (*it)->emit(a...);

                it = itNext;
            }

            this->m_connected_slots.remove_if([this](internal::_connection<args...> *x) {
                if (x->expired) {
                    x->getdest()->signal_disconnect(this);
                    delete x;
                    return true;
                }
                return false;
            });
            // Might need to reconnect new signals. This needs improvement...
            for (auto const conn : this->m_connected_slots) {
                conn->getdest()->signal_connect(this);
            }
        }

        void operator()(args... a)
        {
            this->emit(a...);
        }

#ifndef SIGSLOT_NO_COROUTINES
        auto operator co_await() const {
            return coroutines::awaitable<args...>(const_cast<signal &>(*this));
        }
#endif
    };


#ifndef SIGSLOT_NO_COROUTINES
    namespace coroutines {
        // Generic variant uses a tuple to pass back.
        template<typename... Args>
        struct awaitable : public has_slots {
            ::sigslot::signal<Args...> & signal;
            std::coroutine_handle<> awaiting = nullptr;
            std::optional<std::tuple<Args...>> payload;

            explicit awaitable(::sigslot::signal<Args...> & s) : signal(s) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable const & a) : signal(a.signal), payload(a.payload) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable && other) noexcept : signal(other.signal), payload(std::move(other.payload)) {
                signal.connect(this, &awaitable::resolve);
            }

            bool await_ready() {
                return payload.has_value();
            }

            void await_suspend(std::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
            }

            auto await_resume() {
                return *payload;
            }

            void resolve(Args... a) {
                payload.emplace(a...);
                if (awaiting) ::sigslot::resume_switch(awaiting);
            }
        };

        // Single argument version uses a bare T
        template<typename T>
        struct awaitable<T> : public has_slots {
            ::sigslot::signal<T> & signal;
            std::coroutine_handle<> awaiting = nullptr;
            std::optional<T> payload;
            explicit awaitable(::sigslot::signal<T> & s) : signal(s) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable const & a) : signal(a.signal), payload(a.payload) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable && other) noexcept : signal(other.signal), payload(std::move(other.payload)) {
                signal.connect(this, &awaitable::resolve);
            }

            bool await_ready() {
                return payload.has_value();
            }

            void await_suspend(std::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
            }

            auto await_resume() {
                return *payload;
            }

            void resolve(T a) {
                payload.emplace(a);
                if (awaiting) ::sigslot::resume_switch(awaiting);
            }
        };

        // Single argument reference version uses a bare T &
        template<typename T>
        struct awaitable<T&> : public has_slots {
            ::sigslot::signal<T&> & signal;
            std::coroutine_handle<> awaiting = nullptr;
            T *payload = nullptr;
            explicit awaitable(::sigslot::signal<T&> & s) : signal(s) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable const & a) : signal(a.signal), payload(a.payload) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable && other) noexcept : signal(other.signal), payload(std::move(other.payload)) {
                signal.connect(this, &awaitable::resolve);
            }

            bool await_ready() {
                return payload;
            }

            void await_suspend(std::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
            }

            auto & await_resume() {
                return *payload;
            }

            void resolve(T & a) {
                payload = &a;
                if (awaiting) ::sigslot::resume_switch(awaiting);
            }
        };

        // Zero argument version uses nothing, of course.
        template<>
        struct awaitable<> : public has_slots {
            ::sigslot::signal<> & signal;
            std::coroutine_handle<> awaiting = nullptr;
            bool ready = false;
            explicit awaitable(::sigslot::signal<> & s) : signal(s) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable const & a) : signal(a.signal), ready(a.ready) {
                signal.connect(this, &awaitable::resolve);
            }
            awaitable(awaitable && other) noexcept : signal(other.signal), ready(other.ready) {
                signal.connect(this, &awaitable::resolve);
            }

            bool await_ready() {
                return ready;
            }

            void await_suspend(std::coroutine_handle<> h) {
                // The awaiting coroutine is already suspended.
                awaiting = h;
            }

            void await_resume() {}

            void resolve() {
                ready = true;
                if (awaiting) ::sigslot::resume_switch(awaiting);
            }
        };

    }
#endif
} // namespace sigslot

#endif // SIGSLOT_H__

