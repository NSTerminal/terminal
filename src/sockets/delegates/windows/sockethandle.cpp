// Copyright 2021-2024 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sockets/delegates/sockethandle.hpp"

#include "os/async.hpp"

template <auto Tag>
void Delegates::SocketHandle<Tag>::closeImpl() {
    Async::submit(Async::Shutdown{ { **this, nullptr } });
    Async::submit(Async::Close{ { **this, nullptr } });
}

template <auto Tag>
void Delegates::SocketHandle<Tag>::cancelIO() {
    Async::submit(Async::Cancel{ { **this, nullptr } });
}

template void Delegates::SocketHandle<SocketTag::IP>::closeImpl();
template void Delegates::SocketHandle<SocketTag::IP>::cancelIO();

template void Delegates::SocketHandle<SocketTag::BT>::closeImpl();
template void Delegates::SocketHandle<SocketTag::BT>::cancelIO();
