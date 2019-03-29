// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef TEST_EXPERIMENTAL_TASK_AWAITABLE_TRAITS_HPP
#define TEST_EXPERIMENTAL_TASK_AWAITABLE_TRAITS_HPP

#include <type_traits>
#include <experimental/coroutine>

namespace test_detail {

template<typename Tp>
struct is_coroutine_handle : std::false_type {};

template<typename Tp>
struct is_coroutine_handle<std::experimental::coroutine_handle<Tp>> :
  std::true_type
{};

template<typename Tp>
struct is_valid_await_suspend_result :
  std::disjunction<
    std::is_void<Tp>,
    std::is_same<Tp, bool>,
    is_coroutine_handle<Tp>>
{};

} // namespace test_detail

template<typename Tp, typename = void>
struct is_awaiter : std::false_type {};

template<typename Tp>
struct is_awaiter<Tp, std::void_t<
  decltype(std::declval<Tp&>().await_ready()),
  decltype(std::declval<Tp&>().await_resume()),
  decltype(std::declval<Tp&>().await_suspend(
    std::declval<std::experimental::coroutine_handle<void>>()))>> :
  std::conjunction<
    std::is_same<decltype(std::declval<Tp&>().await_ready()), bool>,
    test_detail::is_valid_await_suspend_result<decltype(
      std::declval<Tp&>().await_suspend(
        std::declval<std::experimental::coroutine_handle<void>>()))>>
{};

template<typename Tp>
constexpr bool is_awaiter_v = is_awaiter<Tp>::value;

namespace test_detail {

template<typename Tp, typename = void>
struct has_member_operator_co_await : std::false_type {};

template<typename Tp>
struct has_member_operator_co_await<Tp, std::void_t<decltype(std::declval<Tp>().operator co_await())>>
: is_awaiter<decltype(std::declval<Tp>().operator co_await())>
{};

template<typename Tp, typename = void>
struct has_non_member_operator_co_await : std::false_type {};

template<typename Tp>
struct has_non_member_operator_co_await<Tp, std::void_t<decltype(operator co_await(std::declval<Tp>()))>>
: is_awaiter<decltype(operator co_await(std::declval<Tp>()))>
{};

} // namespace test_detail

template<typename Tp>
struct is_awaitable : std::disjunction<
  is_awaiter<Tp>,
  test_detail::has_member_operator_co_await<Tp>,
  test_detail::has_non_member_operator_co_await<Tp>>
{};

template<typename Tp>
constexpr bool is_awaitable_v = is_awaitable<Tp>::value;

template<
  typename Tp,
  std::enable_if_t<is_awaitable_v<Tp>, int> = 0>
decltype(auto) get_awaiter(Tp&& awaitable)
{
  if constexpr (test_detail::has_member_operator_co_await<Tp>::value)
  {
    return static_cast<Tp&&>(awaitable).operator co_await();
  }
  else if constexpr (test_detail::has_non_member_operator_co_await<Tp>::value)
  {
    return operator co_await(static_cast<Tp&&>(awaitable));
  }
  else
  {
    return static_cast<Tp&&>(awaitable);
  }
}

template<typename Tp, typename = void>
struct await_result
{};

template<typename Tp>
struct await_result<Tp, std::enable_if_t<is_awaitable_v<Tp>>>
{
private:
  using awaiter = decltype(get_awaiter(std::declval<Tp>()));
public:
  using type = decltype(std::declval<awaiter&>().await_resume());
};

template<typename Tp>
using await_result_t = typename await_result<Tp>::type;

#endif
