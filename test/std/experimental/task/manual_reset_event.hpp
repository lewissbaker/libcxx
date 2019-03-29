// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef TEST_EXPERIMENTAL_TASK_MANUAL_RESET_EVENT_HPP
#define TEST_EXPERIMENTAL_TASK_MANUAL_RESET_EVENT_HPP

#include <experimental/coroutine>
#include <atomic>
#include <cassert>

// manual_reset_event is a coroutine synchronisation tool that allows one
// coroutine to await the event object. If the event is in the 'set' state
// then it continues without suspending, otherwise the coroutine suspends
// until some thread calls .set() on the event.
class manual_reset_event
{
  class awaiter
  {
  public:

    awaiter(const manual_reset_event* event) noexcept
    : event_(event)
    {}

    bool await_ready() const noexcept
    {
      return event_->is_set();
    }

    bool await_suspend(std::experimental::coroutine_handle<> coro) noexcept
    {
      assert(
        (event_->state_.load(std::memory_order_relaxed) !=
        state_t::not_set_waiting_coroutine) &&
        "This manual_reset_event already has another coroutine awaiting it. "
        "Only one awaiting coroutine is supported.");

      event_->awaitingCoroutine_ = coro;

      // If the compare-exchange fails then this means that the event was
      // already 'set' and so we should not suspend - this code path requires
      // 'acquire' semantics so we have visibility of writes prior to the
      // .set() operation that transitioned the event to the 'set' state.
      // If the compare-exchange succeeds then this needs 'release' semantics
      // so that a subsequent call to .set() has visibility of our writes
      // to the coroutine frame and to event_->awaitingCoroutine_ after
      // reading our write to event_->state_.
      state_t oldState = state_t::not_set;
      return event_->state_.compare_exchange_strong(
        oldState,
        state_t::not_set_waiting_coroutine,
        std::memory_order_release,
        std::memory_order_acquire);
    }

    void await_resume() const noexcept {}

  private:
    const manual_reset_event* event_;
  };

public:

  manual_reset_event(bool initiallySet = false) noexcept
  : state_(initiallySet ? state_t::set : state_t::not_set)
  {}

  bool is_set() const noexcept
  {
    return state_.load(std::memory_order_acquire) == state_t::set;
  }

  void set() noexcept
  {
    // Needs to be 'acquire' in case the old value was a waiting coroutine
    // so that we have visibility of the writes to the coroutine frame in
    // the current thrad before we resume it.
    // Also needs to be 'release' in case the old value was 'not-set' so that
    // another thread that subsequently awaits the
    state_t oldState = state_.exchange(state_t::set, std::memory_order_acq_rel);
    if (oldState == state_t::not_set_waiting_coroutine)
    {
      std::exchange(awaitingCoroutine_, {}).resume();
    }
  }

  void reset() noexcept
  {
    assert(
      (state_.load(std::memory_order_relaxed) != state_t::not_set_waiting_coroutine) &&
      "Illegal to call reset() if a coroutine is currently awaiting the event.");

    // Note, we use 'relaxed' memory order here since it considered a
    // data-race to call reset() concurrently either with operator co_await()
    // or with set().
    state_.store(state_t::not_set, std::memory_order_relaxed);
  }

  awaiter operator co_await() const noexcept
  {
    return awaiter{ this };
  }

private:

  enum class state_t {
    not_set,
    not_set_waiting_coroutine,
    set
  };

  // TODO: Can we combine these two members into a single std::atomic<void*>?
  mutable std::atomic<state_t> state_;
  mutable std::experimental::coroutine_handle<> awaitingCoroutine_;

};

#endif
