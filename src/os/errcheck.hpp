// Copyright 2021-2022 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <string_view>

#include "error.hpp"

#define FN(f, ...) SysFn{ #f, f __VA_OPT__(,) __VA_ARGS__ }

template <class Fn, class... Args> requires std::invocable<Fn, Args...>
struct SysFn {
    std::string name;
    std::function<Fn> fn;

    explicit SysFn(std::string_view s, Fn p, Args&&... args) : name(s), fn(std::bind(p, std::forward<Args>(args)...)) {}

    auto operator()() { return fn(); }
};

// Calls a system function, and throws an exception if its return code does not match a success value.
//
// The thrown exception can be modified with type and errorValue. Since this macro is defined as a lambda, the
// function's return code can also be accessed.
#define CALL_FN_BASE(f, type, successCheck, errorValue, ...)                                          \
    [&] {                                                                                             \
        auto rc = f(__VA_ARGS__);                                                                     \
        return ((successCheck) || !System::isFatal(errorValue))                                       \
                 ? rc                                                                                 \
                 : throw System::SystemError{ static_cast<System::ErrorCode>(errorValue), type, #f }; \
    }()

// Calls one of the *_TYPE macros with the type being System.
#define CALL_WITH_DEFAULT_TYPE(macro, f, ...) macro(f, System::ErrorType::System, __VA_ARGS__)

// Throws if a function fails.
#define CALL_FN_TYPE(f, type, check, ...) CALL_FN_BASE(f, type, check, System::getLastError(), __VA_ARGS__)

// Throws if the return code is not true.
#define EXPECT_TRUE_TYPE(f, type, ...) CALL_FN_TYPE(f, type, rc, __VA_ARGS__)
#define EXPECT_TRUE(f, ...) CALL_WITH_DEFAULT_TYPE(EXPECT_TRUE_TYPE, f, __VA_ARGS__)

// Throws if the return code is not zero.
#define EXPECT_ZERO_TYPE(f, type, ...) CALL_FN_TYPE(f, type, rc == NO_ERROR, __VA_ARGS__)
#define EXPECT_ZERO(f, ...) CALL_WITH_DEFAULT_TYPE(EXPECT_ZERO_TYPE, f, __VA_ARGS__)

// Throws if the return code is equal to SOCKET_ERROR (-1).
#define EXPECT_NONERROR_TYPE(f, type, ...) CALL_FN_TYPE(f, type, rc != SOCKET_ERROR, __VA_ARGS__)
#define EXPECT_NONERROR(f, ...) CALL_WITH_DEFAULT_TYPE(EXPECT_NONERROR_TYPE, f, __VA_ARGS__)

// Throws if the return code is not true.
// The exception's error value will be set to the function's return value.
#define EXPECT_ZERO_RC_TYPE(f, type, ...) CALL_FN_BASE(f, type, rc == NO_ERROR, rc, __VA_ARGS__)
#define EXPECT_ZERO_RC(f, ...) CALL_WITH_DEFAULT_TYPE(EXPECT_ZERO_RC_TYPE, f, __VA_ARGS__)

// Throws if the return code is negative.
// The exception's error value will be set to the function's return value, negated. This macro can be used with newer
// POSIX APIs that return >= 0 on success and -errno on failure.
#define EXPECT_POSITIVE_RC_TYPE(f, type, ...) CALL_FN_BASE(f, type, rc >= 0, -rc, __VA_ARGS__)
#define EXPECT_POSITIVE_RC(f, ...) CALL_WITH_DEFAULT_TYPE(EXPECT_POSITIVE_RC_TYPE, f, __VA_ARGS__)
