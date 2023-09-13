// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <coroutine>
#include <memory>
#include <stdexcept>

#include "delegates.hpp"
#include "sockethandle.hpp"
#include "traits.hpp"

#include "net/enums.hpp"

#if OS_WINDOWS
#if __clang__
#define NO_UNIQUE_ADDRESS // Not supported on Clang on Windows
#else
#define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#endif
#else
#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace Delegates {
    template <auto Tag>
    class Server : public ServerDelegate {
        SocketHandle<Tag>& _handle;
        NO_UNIQUE_ADDRESS Traits::Server<Tag> _traits;

        [[noreturn]] static void _throwUnsupported() {
            throw std::invalid_argument{ "Operation not supported with socket type" };
        }

    public:
        explicit Server(SocketHandle<Tag>& handle) : _handle(handle) {}

        ServerAddress startServer(const Device& serverInfo) override;

        Task<AcceptResult> accept() override;

        Task<DgramRecvResult> recvFrom(size_t) override {
            co_return {};
        }

        Task<> sendTo(std::string, std::string) override {
            co_return;
        }
    };
}

// There are no connectionless operations on Bluetooth sockets

template <>
inline Task<DgramRecvResult> Delegates::Server<SocketTag::BT>::recvFrom(size_t) {
    _throwUnsupported();
}

template <>
inline Task<> Delegates::Server<SocketTag::BT>::sendTo(std::string, std::string) {
    co_await std::suspend_never{}; // Indicate this is a coroutine and strings should be passed by value
    _throwUnsupported();
}
