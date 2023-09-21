// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#if OS_MACOS
#include "bidirectional.hpp"

#include "net/enums.hpp"
#include "os/async_macos.hpp"
#include "os/error.hpp"

template <>
Task<> Delegates::Bidirectional<SocketTag::BT>::send(std::string data) {
    [*handle write:data];
    co_await Async::run(std::bind_front(Async::submitIOBluetooth, [*handle channelHash], Async::IOType::Send),
                        System::ErrorType::IOReturn);
}

template <>
Task<RecvResult> Delegates::Bidirectional<SocketTag::BT>::recv(size_t) {
    co_await Async::run(std::bind_front(Async::submitIOBluetooth, [*handle channelHash], Async::IOType::Receive),
                        System::ErrorType::IOReturn);

    co_return Async::getBluetoothReadResult([*handle channelHash]);
}
#endif
