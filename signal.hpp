#pragma once
#include <memory>
#include <vector>
#include <algorithm>
#include <atomic>
#include <functional>


namespace detail
{
struct Spin
{
    std::atomic_flag disconnecting = ATOMIC_FLAG_INIT;
};
template<typename Callable>
struct SpinAction : Spin
{
    //TODO maybe use something nice and lightweight in the future instead of std::function
    using ActionType = std::function<Callable>;
    ActionType func;
};

struct TrivialBase
{
    struct
    {

    } m_connections;
};

}

class [[nodiscard]] Connection
{
    class Action
    {
        friend class Connection;
        Action() = default;
        Action(Action&&) = default;
        Action& operator=(Action&&) = default;
        detail::Spin* operator->() { return ptr.get(); };
        std::shared_ptr<detail::Spin> ptr;
    } m_action;

protected:
    template<typename T>
    Connection(std::shared_ptr<detail::SpinAction<T>>&& func) {m_action.ptr = std::move(func);}
    template<typename, typename>
    friend class Signal;

public:
    Connection() = default;
    Connection(Action&& action)
    {
        disconnect();
        m_action = std::move(action);
        if(m_action.ptr)
            m_action->disconnecting.clear(std::memory_order_release);
    }
    Connection& operator=(Action&& action)
    {
        disconnect();
        m_action = std::move(action);
        if(m_action.ptr)
            m_action->disconnecting.clear(std::memory_order_release);
        return *this;
    }

    Connection(const Connection&) = delete;
    Connection(Connection&& other)
    {
        //not interested in locking here - this might only become a problem when different threads are moving Connection object
        //and trying to disconnect() it at the same time - does not sound like reasonable usecase
        std::swap(m_action.ptr, other.m_action.ptr);
    }
    Connection& operator=(const Connection&) = delete;
    Connection& operator=(Connection&& other)
    {
        //not interested in locking here - this might only become a problem when different threads are moving Connection object
        //and trying to disconnect() it at the same time - does not sound like reasonable usecase
        std::swap(m_action.ptr, other.m_action.ptr);
        return *this;
    }

    bool isConnected() {return m_action.ptr != nullptr;}

    void disconnect()
    {
        while(m_action.ptr && m_action->disconnecting.test_and_set(std::memory_order_acquire))
        {
            //noop
        }
        m_action.ptr = nullptr;
    }

    //while Action is detached from Connection all callbacks will be silently ignored
    //after using release() it is user's responsibility to make sure corresponding Signal remains valid after reattach
    Action release()
    {
        while(m_action.ptr && m_action->disconnecting.test_and_set(std::memory_order_acquire))
        {
            //noop
        }
        Action released = std::move(m_action);
        m_action.ptr = nullptr;
        return released;
    }

    ~Connection()
    {
        disconnect();
    }
};

//TODO use const to force user into using release() instead of this
class NonMovableConnection : protected Connection
{
public:
    using Connection::Connection;
    using Connection::operator=;
    NonMovableConnection(Connection&& c): Connection(std::move(c)) {}
    NonMovableConnection(NonMovableConnection&&) = delete;
    NonMovableConnection& operator=(NonMovableConnection&&) = delete;
    using Connection::isConnected;
    using Connection::disconnect;
    using Connection::release;
};

class NonMovableConnectionContainer
{
    std::vector<Connection> m_container;
public:
    NonMovableConnectionContainer() = default;
    NonMovableConnectionContainer(const NonMovableConnectionContainer&) = delete;
    NonMovableConnectionContainer& operator=(const NonMovableConnectionContainer&) = delete;
    NonMovableConnectionContainer(NonMovableConnectionContainer&&) = delete;
    NonMovableConnectionContainer& operator=(NonMovableConnectionContainer&&) = delete;

    void push_back(NonMovableConnection&& conn)
    {
        m_container.push_back(conn.release());
    }
};

class TypeErasedSignal
{
    //if you are absolutely sure about underlying type static_cast to Signal can be used
    //otherwise use dynamic_cast, or don't use this class at all
protected:
    std::vector<std::weak_ptr<void>> m_connections;
};

template<typename Callable, typename Base = TypeErasedSignal>
class Signal final : public Base
{
    using Base::m_connections;
    void hereBeAsserts()
    {
        //TODO concepts?
        using detail::TrivialBase;
        static_assert(sizeof(Signal<Callable>) == sizeof(TypeErasedSignal), "Signal<Callable> must not have data members!");
        static_assert(std::is_same_v<Base, TypeErasedSignal> || std::is_same_v<Base, TrivialBase>,
                      "Inherit only TypeErasedSignal in Signal<Callable>");
        static_assert(std::is_trivially_destructible_v<Signal<Callable, TrivialBase>>,
                      "Signal<Callable> must be trivially destructible, so Base class can have non-virtual destructor");
        static_assert(std::is_standard_layout_v<Signal<Callable>> &&
                      std::is_standard_layout_v<Signal<Callable, TrivialBase>>);
    }
public:
    //movable Connection returned for free functions and captureless lambdas, non-movable for anything else
    template <typename F>
    auto connect(F&& func) -> std::conditional_t<std::is_function_v<F> || std::is_convertible_v<F, Callable*>,
                                                 Connection, NonMovableConnection>
    {
        using detail::SpinAction;
        std::shared_ptr<SpinAction<Callable>> action{new SpinAction<Callable>{ATOMIC_FLAG_INIT, std::move(func)}};
            m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(),
                                               [](auto&& weak){return weak.expired();}), m_connections.end());
            m_connections.emplace_back(action);
        return {std::move(action)};
    };

    template<typename ...Args>
    void operator()(Args&&... args) const
    {
        using detail::SpinAction;
        for(auto&& weak: m_connections)
        {
            if(auto shared = std::static_pointer_cast<SpinAction<Callable>>(weak.lock());
               shared && ! shared->disconnecting.test_and_set(std::memory_order_acquire))
            {
                static_assert(std::is_invocable_v<decltype(shared->func), Args...>);
                std::invoke(shared->func, std::forward<Args>(args)...);
                shared->disconnecting.clear(std::memory_order_release);
            }
        }
    }
};

