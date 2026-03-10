// include/toy/execution.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_INCLUDE_TOY_EXECUTION
#define INCLUDED_INCLUDE_TOY_EXECUTION

#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

// ----------------------------------------------------------------------------

namespace toy {
    // Customization points
    inline constexpr struct set_value_t {
        template <typename Receiver>
        auto operator()(Receiver receiver, auto value) const noexcept
        {
            return receiver.set_value(std::move(value));
        }
    } set_value{};
    inline constexpr struct set_error_t {
        template <typename Receiver>
        auto operator()(Receiver receiver, auto value) const noexcept
        {
            return receiver.set_error(std::move(value));
        }
    } set_error{};
    inline constexpr struct set_stopped_t {
        template <typename Receiver>
        auto operator()(Receiver receiver) const noexcept
        {
            return receiver.set_stopped();
        }
    } set_stopped{};

    inline constexpr struct connect_t {
        auto operator()(auto sender, auto receiver) const noexcept
        {
            return sender.connect(std::move(receiver));
        }
    } connect{};
    inline constexpr struct start_t {
        auto operator()(auto& state) const noexcept
        {
            return state.start();
        }
    } start{};

    // Helper
    template <typename Sender, typename Receiver>
    struct state_type {
        struct connector {
            using state = decltype(connect(std::declval<Sender>(), std::declval<Receiver>()));
            state s;
            connector(auto sender, auto receiver) noexcept
                : s(connect(std::move(sender), std::move(receiver))) {}
        };
        std::optional<connector> c;
        template <typename S, typename R>
        auto operator()(S sender, R receiver) noexcept
        {
            c.emplace(std::move(sender), std::move(receiver));
        }
        auto start() noexcept
        {
            assert(c);
            return toy::start(c->s);
        }
    };

    // Sender factory
    template <typename Tag>
    struct just_t {
        template <typename Value>
        struct sender {
            using type = Value;
            template <typename Receiver>
            struct state {
                Receiver receiver;
                Value value;
                auto start() noexcept
                {
                    return Tag{}(std::move(receiver), std::move(value));
                }
            };

            Value value;
            template <typename Receiver>
            auto connect(Receiver receiver) const noexcept
            {
                return state<Receiver>{std::move(receiver), value};
            }
        };
        template <typename Value>
        auto operator()(Value value) const noexcept { return sender<Value>{std::move(value)}; }
    };

    inline constexpr just_t<set_value_t> just{};
    inline constexpr just_t<set_error_t> just_error{};
    inline constexpr just_t<set_stopped_t> just_stopped{};

    // Sender consumer
    inline constexpr struct sync_wait
    {
        template <typename T>
        struct receiver {
            struct data {
            std::optional<T> value;
            std::optional<std::exception_ptr> error;
            bool stopped = false;
            };

            data* d;
            auto set_value(auto v) { d->value = v; return 0; }
            auto set_error(auto e) { d->error = e; return 0; }
            auto set_stopped() { d->stopped = true; return 0; }
        };

        template <typename Sender>
        auto operator()(Sender sender) const
        {
        using rcvr = receiver<typename Sender::type>;
        typename rcvr::data r;
        auto state = connect(sender, rcvr(&r));
        start(state);
        if (r.value) return *r.value;
        if (r.error) throw *r.error;
        if (r.stopped) throw std::runtime_error("stopped");
        throw std::runtime_error("unexpected");
        }
    } sync_wait{};

    // Task
    template <typename T>
    struct task {
        using type = T;

        struct state_base {
            virtual ~state_base() = default; 
            virtual void complete() = 0;
            std::optional<T> value{};
            std::optional<std::exception_ptr> error{};
            bool stopped = false;
        };
        template <typename Receiver>
        struct state
        : state_base
        {
            std::coroutine_handle<> handle;
            Receiver receiver;

            state(std::coroutine_handle<> h, Receiver r, state_base*& s)
                : handle(h), receiver(std::move(r)){
                s = this;
            }

            auto start() noexcept {
                handle.resume();
            }
            void complete() override {
                if (this->value) set_value(receiver, std::move(*this->value));
                else if (this->error) set_error(receiver, std::move(*this->error));
                else if (this-> stopped) set_stopped(receiver);
                else throw std::runtime_error("unexpected");
            }
        };

        struct promise_type {
            state_base* s{};

            auto initial_suspend() { return std::suspend_always{}; }
            auto final_suspend() noexcept {
                struct awaiter{
                    state_base* s;
                    bool await_ready() noexcept { return false; }
                    void await_suspend(std::coroutine_handle<promise_type> h) noexcept { s->complete(); }
                    void await_resume() noexcept {}
                };
                return awaiter{this->s};
            }
            auto get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
            auto return_value(T v) { s->value = std::move(v); }
            auto unhandled_exception() { s->error = std::current_exception(); }

            template <typename V>
            struct receiver {
                struct data {
                    std::coroutine_handle<> handle;
                    std::optional<V> value;
                    std::optional<std::exception_ptr> error;
                    bool stopped = false;
                };

                data* d;
                auto set_value(auto v) {
                    std::cout << "coro::set_value(" << v << ")\n";
                    d->value = v;
                    return this->d->handle;
                }
                auto set_error(auto e) { std::cout << "coro::set_error()\n"; d->error = e; return this->d->handle; }
                auto set_stopped() { std::cout << "coro::set_stopped()\n"; d->stopped = true; return this->d->handle; }
            };
            template <typename Sender>
            struct awaiter {
                using rcvr = receiver<typename Sender::type>;
                Sender sender;
                typename rcvr::data r;
                state_type<Sender, rcvr> state;

                constexpr bool await_ready() const noexcept { return false; }
                template <typename Promise>
                auto await_suspend(std::coroutine_handle<Promise> h) noexcept -> std::coroutine_handle<>
                {
                    r.handle = std::move(h);
                    this->state(std::move(sender), rcvr(&r));
                    return start(this->state);
                }
                auto await_resume() const {
                    if (r.value) return *r.value;
                    else if (r.error) std::rethrow_exception(*r.error);
                    else if (r.stopped) throw std::runtime_error("stopped");
                    else throw std::runtime_error("unexpected");
                }
            };
            template <typename Sender>
            auto await_transform(Sender sender) {
                return awaiter<Sender>{std::move(sender)};
            }
        };

        std::coroutine_handle<promise_type> handle;
        template <typename Receiver>
        auto connect(Receiver receiver) const noexcept
        {
            return state<Receiver>(std::move(handle), std::move(receiver), handle.promise().s);
        }
    };
}

// ----------------------------------------------------------------------------

#endif
