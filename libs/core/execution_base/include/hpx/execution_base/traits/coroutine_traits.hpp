//  Copyright (c) 2022 Shreyas Atre
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#if defined(HPX_HAVE_CXX20_COROUTINES)

#include <hpx/config.hpp>
#include <hpx/modules/concepts.hpp>
#include <hpx/type_support/meta.hpp>

#include <type_traits>
#include <utility>

namespace hpx { namespace execution { namespace experimental {

    template <typename Promise, typename Awaiter>
    decltype(auto) await_suspend(Awaiter&& await)
    {
        if constexpr (!std::is_void_v<Promise>)
        {
            return await.await_suspend(hpx::coro::coroutine_handle<Promise>{});
        }
        else
        {
            return;
        }
    }

    namespace detail {

        template <typename, template <typename...> typename>
        inline constexpr bool is_instance_of_ = false;

        template <typename... As, template <typename...> typename T>
        inline constexpr bool is_instance_of_<T<As...>, T> = true;

        template <typename T, template <typename...> typename F>
        inline constexpr bool is_instance_of = is_instance_of_<T, F>;

        template <typename T>
        inline constexpr bool is_await_suspend_result_v =
            meta::value<meta::one_of<T, void, bool>> ||
            is_instance_of<T, hpx::coro::coroutine_handle>;

        template <typename, typename = void>
        inline constexpr bool has_await_ready = false;

        template <typename T>
        inline constexpr bool has_await_ready<T,
            std::void_t<decltype(std::declval<T>().await_ready())>> = true;

        template <typename, typename = void>
        inline constexpr bool has_await_resume = false;

        template <typename T>
        inline constexpr bool has_await_resume<T,
            std::void_t<decltype(std::declval<T>().await_resume())>> = true;

        template <typename, typename, typename = void>
        inline constexpr bool has_await_suspend_coro_handle = false;

        template <typename T, typename Ts>
        inline constexpr bool has_await_suspend_coro_handle<T, Ts,
            std::void_t<decltype(std::declval<T>().await_suspend(
                hpx::coro::coroutine_handle<Ts>{}))>> = true;

        template <bool await_ready, typename Awaiter, typename Promise>
        struct is_awaiter_impl;

        template <typename Awaiter, typename Promise>
        struct is_awaiter_impl<false, Awaiter, Promise> : std::false_type
        {
        };

        // different versions of clang-format disagree
        // clang-format off
        template <typename Awaiter, typename Promise>
        struct is_awaiter_impl<true, Awaiter, Promise>
          : std::integral_constant<bool,
                is_await_suspend_result_v<decltype(await_suspend<Promise>(
                    std::declval<Awaiter>()))>>
        // clang-format on
        {
        };

    }    // namespace detail

    // An Awaiter type is a type that implements the three special methods that
    // are called as part of a co_await expression: await_ready, await_suspend
    // and await_resume.
    //
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1288r0.pdf
    // Lewis Baker - The rationale here is that for an awaiter object to be able
    // to support being awaited in an arbitrary natural coroutine context it
    // will generally need to type-erase the coroutine_handle<Promise> to
    // coroutine_handle<void> so that it can store the continuation for an
    // arbitrary coroutine-type. If the await_suspend() method overload-set only
    // has overloads that accept specific types of coroutine_handle<P> then it
    // is only awaitable within specific contexts and thus we don't consider it
    // to satisfy the Awaiter concept.
    //
    template <typename Awaiter, typename Promise = void>
    struct is_awaiter
      : detail::is_awaiter_impl<detail::has_await_ready<Awaiter> &&
                detail::has_await_resume<Awaiter> &&
                detail::has_await_suspend_coro_handle<Awaiter, Promise>,
            Awaiter, Promise>
    {
    };

    template <typename Awaiter, typename Promise = void>
    inline constexpr bool is_awaiter_v = is_awaiter<Awaiter, Promise>::value;

    namespace detail {

        template <typename Awaitable, typename = void>
        inline constexpr bool has_member_operator_co_await_v = false;

        // different versions of clang-format disagree
        // clang-format off
        template <typename Awaitable>
        inline constexpr bool has_member_operator_co_await_v<Awaitable,
            std::void_t<
                decltype(std::declval<Awaitable>().operator co_await())>> =
            true;
        // clang-format on

        template <typename Awaitable, typename = void>
        inline constexpr bool has_free_operator_co_await_v = false;

        template <typename Awaitable>
        inline constexpr bool has_free_operator_co_await_v<Awaitable,
            std::void_t<decltype(operator co_await(
                std::declval<Awaitable>()))>> = true;

        HPX_HAS_MEMBER_XXX_TRAIT_DEF(await_transform);

    }    // namespace detail

    // Returns the result of applying operator co_await() to the function's
    // argument, if the operator is defined, otherwise returns a reference to
    // the input argument.
    template <typename Awaitable>
    decltype(auto) get_awaiter(Awaitable&& await, void*)
    {
        if constexpr (detail::has_member_operator_co_await_v<Awaitable>)
        {
            return HPX_FORWARD(Awaitable, await).operator co_await();
        }
        else if constexpr (detail::has_free_operator_co_await_v<Awaitable>)
        {
            return operator co_await(HPX_FORWARD(Awaitable, await));
        }
        else
        {
            return HPX_FORWARD(Awaitable, await);
        }
    }

    template <typename Awaitable, typename Promise,
        typename = std::enable_if_t<detail::has_await_transform_v<Promise>>>
    decltype(auto) get_awaiter(Awaitable&& await, Promise* promise)
    {
        // different versions of clang-format disagree
        // clang-format off
        if constexpr (detail::has_member_operator_co_await_v<
                          decltype(promise->await_transform(
                              HPX_FORWARD(Awaitable, await)))>)
        {
            return promise->await_transform(HPX_FORWARD(Awaitable, await))
                .operator co_await();
        }
        else if constexpr (detail::has_free_operator_co_await_v<
                               decltype(promise->await_transform(
                                   HPX_FORWARD(Awaitable, await)))>)
        {
            return operator co_await(
                promise->await_transform(HPX_FORWARD(Awaitable, await)));
        }
        else
        {
            return promise->await_transform(HPX_FORWARD(Awaitable, await));
        }
        // clang-format on
    }

    // Awaitable - Something that you can apply the 'co_await' operator to. If
    // the promise type defines an await_transform() member then the awaitable
    // is obtained by calling promise.await_transform(value), passing the
    // awaited value.
    //
    // Otherwise, if the promise type does not define an await_transform()
    // member then the awaitable is the awaited value itself.
    //
    // The awaitable concept simply checks whether the type supports applying
    // the co_await operator to avalue of that type. If the object has either a
    // member or non-member operator co_await() then its return value must
    // satisfy the Awaiter concept. Otherwise, the Awaitable object must satisfy
    // the Awaiter concept itself.
    //
    template <typename Awaitable, typename Promise = void>
    struct is_awaitable
      : is_awaiter<decltype(get_awaiter(std::declval<Awaitable>(),
                       static_cast<Promise*>(nullptr))),
            Promise>
    {
    };

    template <typename Awaitable, typename Promise = void>
    inline constexpr bool is_awaitable_v =
        is_awaitable<Awaitable, Promise>::value;

    // different versions of clang-format disagree
    // clang-format off
    template <typename Awaitable, typename Promise = void,
        typename = std::enable_if_t<is_awaitable_v<Awaitable, Promise>>>
    using await_result_t = decltype((
        get_awaiter(std::declval<Awaitable>(), static_cast<Promise*>(nullptr))
            .await_resume()));
    // clang-format on

}}}    // namespace hpx::execution::experimental

#endif    // HPX_HAVE_CXX20_COROUTINES
