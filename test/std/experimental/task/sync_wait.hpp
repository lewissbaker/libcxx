// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef TEST_EXPERIMENTAL_TASK_SYNC_WAIT_HPP
#define TEST_EXPERIMENTAL_TASK_SYNC_WAIT_HPP

#include <experimental/__config>
#include <experimental/coroutine>
#include <type_traits>
#include <mutex>
#include <condition_variable>
#include <cassert>

#include "awaitable_traits.hpp"
#include "test_macros.h"

namespace test_detail {

// Thread-synchronisation helper that allows one thread to block in a call
// to .wait() until another thread signals the thread by calling .set().
class oneshot_event
{
public:
  oneshot_event() : isSet_(false) {}

  void set() noexcept
  {
    std::unique_lock<std::mutex> lock{ mutex_ };
    isSet_ = true;
    cv_.notify_all();
  }

  void wait() noexcept
  {
    std::unique_lock<std::mutex> lock{ mutex_ };
    cv_.wait(lock, [this] { return isSet_; });
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool isSet_;
};

template<typename Derived>
class sync_wait_promise_base
{
public:

  using handle_t = std::experimental::coroutine_handle<Derived>;

private:

  struct FinalAwaiter
  {
      bool await_ready() noexcept { return false; }
      void await_suspend(handle_t coro) noexcept
      {
        sync_wait_promise_base& promise = coro.promise();
        promise.event_.set();
      }
      void await_resume() noexcept {}
  };

public:

  handle_t get_return_object() { return handle(); }
  std::experimental::suspend_always initial_suspend() { return {}; }
  FinalAwaiter final_suspend() { return {}; }

private:

  handle_t handle() noexcept
  {
    return handle_t::from_promise(static_cast<Derived&>(*this));
  }

protected:

  // Start the coroutine and then block waiting for it to finish.
  void run() noexcept
  {
    handle().resume();
    event_.wait();
  }

private:

  oneshot_event event_;

};

template<typename Tp>
class sync_wait_promise final
  : public sync_wait_promise_base<sync_wait_promise<Tp>>
{
public:

  sync_wait_promise() : state_(state_t::empty) {}

  ~sync_wait_promise()
  {
    switch (state_)
    {
      case state_t::empty:
      case state_t::value:
        break;
      case state_t::exception:
#ifndef TEST_HAS_NO_EXCEPTIONS
        exception_.~exception_ptr();
#endif
        break;
    }
  }

  void return_void() noexcept
  {
    // Should be unreachable since coroutine should always
    // suspend at `co_yield` point where it will be destroyed
    // or will fail with an exception and bypass return_void()
    // and call unhandled_exception() instead.
    std::abort();
  }

  void unhandled_exception() noexcept
  {
#ifndef TEST_HAS_NO_EXCEPTIONS
    ::new (static_cast<void*>(&exception_)) std::exception_ptr(
      std::current_exception());
    state_ = state_t::exception;
#else
    std::abort();
#endif
  }

  auto yield_value(Tp&& value) noexcept
  {
    valuePtr_ = std::addressof(value);
    state_ = state_t::value;
    return this->final_suspend();
  }

  Tp&& get()
  {
    this->run();

#ifndef TEST_HAS_NO_EXCEPTIONS
    if (state_ == state_t::exception)
    {
      std::rethrow_exception(std::move(exception_));
    }
#endif

    assert(state_ == state_t::value);
    return static_cast<Tp&&>(*valuePtr_);
  }

private:

  enum class state_t {
    empty,
    value,
    exception
  };

  state_t state_;
  union {
    std::add_pointer_t<Tp> valuePtr_;
    std::exception_ptr exception_;
  };

};

template<>
struct sync_wait_promise<void> final
  : public sync_wait_promise_base<sync_wait_promise<void>>
{
public:

  void unhandled_exception() noexcept
  {
#ifndef TEST_HAS_NO_EXCEPTIONS
    exception_ = std::current_exception();
#endif
  }

  void return_void() noexcept {}

  void get()
  {
    this->run();

#ifndef TEST_HAS_NO_EXCEPTIONS
    if (exception_)
    {
      std::rethrow_exception(std::move(exception_));
    }
#endif
  }

private:

  std::exception_ptr exception_;

};

template<typename Tp>
class sync_wait_task final
{
public:
  using promise_type = sync_wait_promise<Tp>;

private:
  using handle_t = typename promise_type::handle_t;

public:

  sync_wait_task(handle_t coro) noexcept : coro_(coro) {}

  ~sync_wait_task()
  {
    assert(coro_ && "Should always have a valid coroutine handle");
    coro_.destroy();
  }

  decltype(auto) get()
  {
    return coro_.promise().get();
  }
private:
  handle_t coro_;
};

template<typename Tp>
struct remove_rvalue_reference
{
  using type = Tp;
};

template<typename Tp>
struct remove_rvalue_reference<Tp&&>
{
  using type = Tp;
};

template<typename Tp>
using remove_rvalue_reference_t =
  typename remove_rvalue_reference<Tp>::type;

template<
  typename Awaitable,
  typename AwaitResult = await_result_t<Awaitable>,
  std::enable_if_t<std::is_void_v<AwaitResult>, int> = 0>
sync_wait_task<AwaitResult> make_sync_wait_task(Awaitable&& awaitable)
{
  co_await static_cast<Awaitable&&>(awaitable);
}

template<
  typename Awaitable,
  typename AwaitResult = await_result_t<Awaitable>,
  std::enable_if_t<!std::is_void_v<AwaitResult>, int> = 0>
sync_wait_task<AwaitResult> make_sync_wait_task(Awaitable&& awaitable)
{
  co_yield co_await static_cast<Awaitable&&>(awaitable);
}

} // namespace test_detail

template<typename Awaitable>
auto sync_wait(Awaitable&& awaitable)
  -> test_detail::remove_rvalue_reference_t<await_result_t<Awaitable>>
{
  return test_detail::make_sync_wait_task(
    static_cast<Awaitable&&>(awaitable)).get();
}

#endif
