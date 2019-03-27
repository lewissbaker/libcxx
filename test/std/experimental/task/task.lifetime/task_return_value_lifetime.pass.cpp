// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++98, c++03, c++11, c++14

#include <experimental/task>
#include <cassert>
#include <iostream>

#include "../counted.hpp"
#include "../sync_wait.hpp"

DEFINE_COUNTED_VARIABLES();

void test_return_value_lifetime()
{
  counted::reset();

  auto f = [](bool x) -> std::experimental::task<counted>
  {
    if (x) {
      counted c;
      co_return std::move(c);
    }
    co_return {};
  };

  {
    auto t = f(true);

    assert(counted::active_instance_count() == 0);
    assert(counted::copy_constructor_count() == 0);
    assert(counted::move_constructor_count() == 0);

    {
      auto c = sync_wait(std::move(t));
      assert(c.id() == 1);

      assert(counted::active_instance_count() == 2);
      assert(counted::copy_constructor_count() == 0);
      assert(counted::move_constructor_count() > 0);
      assert(counted::default_constructor_count() == 1);
    }

    // The result value in 't' is still alive until 't' destructs.
    assert(counted::active_instance_count() == 1);
  }

  assert(counted::active_instance_count() == 0);

  counted::reset();

  {
    auto t = f(false);

    assert(counted::active_instance_count() == 0);
    assert(counted::copy_constructor_count() == 0);
    assert(counted::move_constructor_count() == 0);

    {
      auto c = sync_wait(std::move(t));
      assert(c.id() == 1);

      assert(counted::active_instance_count() == 2);
      assert(counted::copy_constructor_count() == 0);
      assert(counted::move_constructor_count() > 0);
      assert(counted::default_constructor_count() == 1);
    }

    // The result value in 't' is still alive until 't' destructs.
    assert(counted::active_instance_count() == 1);
  }
}

struct my_error {};

struct throws_on_destruction
{
  ~throws_on_destruction() noexcept(false)
  {
    throw my_error{};
  }
};

void test_uncaught_exception_thrown_after_co_return()
{
  counted::reset();

  assert(counted::active_instance_count() == 0);
  assert(counted::copy_constructor_count() == 0);
  assert(counted::move_constructor_count() == 0);

  {
    auto t = []() -> std::experimental::task<counted>
    {
      throws_on_destruction d;
      co_return counted{};
    }();

    try {
      (void)sync_wait(std::move(t));
      assert(false);
    } catch (const my_error&) {
    }

    assert(counted::active_instance_count() == 0);
    assert(counted::copy_constructor_count() == 0);
    assert(counted::move_constructor_count() > 0);
    assert(counted::default_constructor_count() == 1);
  }

  assert(counted::active_instance_count() == 0);
}

void test_exception_thrown_and_caught_after_co_return()
{
  counted::reset();

  assert(counted::active_instance_count() == 0);
  assert(counted::copy_constructor_count() == 0);
  assert(counted::move_constructor_count() == 0);

  {
    auto t = []() -> std::experimental::task<counted>
    {
      try {
        throws_on_destruction d;
        co_return counted{};
      } catch(const my_error&) {
        co_return counted{};
      }
    }();

    auto c = sync_wait(std::move(t));
    assert(c.id() == 2);

    assert(counted::active_instance_count() == 2);
    assert(counted::copy_constructor_count() == 0);
    assert(counted::move_constructor_count() > 0);
    assert(counted::default_constructor_count() == 2);
  }

  assert(counted::active_instance_count() == 0);
}

int main()
{
  test_return_value_lifetime();
  test_uncaught_exception_thrown_after_co_return();
  test_exception_thrown_and_caught_after_co_return();
  return 0;
}
