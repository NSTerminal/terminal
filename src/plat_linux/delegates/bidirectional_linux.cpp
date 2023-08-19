// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#if OS_LINUX
#include <liburing.h>

#include "os/errcheck.hpp"
#include "plat_linux/async_linux.hpp"
#include "sockets/delegates/bidirectional.hpp"

template <auto Tag>
Task<> Delegates::Bidirectional<Tag>::send(std::string data) {
    co_await Async::run([this, &data](Async::CompletionResult& result) {
        io_uring_sqe* sqe = Async::getUringSQE();
        io_uring_prep_send(sqe, _handle, data.data(), data.size(), MSG_NOSIGNAL);
        io_uring_sqe_set_data(sqe, &result);
        Async::submitRing();
    });
}

template <auto Tag>
Task<RecvResult> Delegates::Bidirectional<Tag>::recv() {
    std::string data(_recvLen, 0);

    auto recvResult = co_await Async::run([this, &data](Async::CompletionResult& result) {
        io_uring_sqe* sqe = Async::getUringSQE();
        io_uring_prep_recv(sqe, _handle, data.data(), _recvLen, MSG_NOSIGNAL);
        io_uring_sqe_set_data(sqe, &result);
        Async::submitRing();
    });

    if (recvResult.res == 0) co_return std::nullopt;

    data.resize(recvResult.res);
    co_return data;
}

template <auto Tag>
void Delegates::Bidirectional<Tag>::cancelIO() {
    Async::cancelPending(_handle);
}

template Task<> Delegates::Bidirectional<SocketTag::IP>::send(std::string);
template Task<RecvResult> Delegates::Bidirectional<SocketTag::IP>::recv();
template void Delegates::Bidirectional<SocketTag::IP>::cancelIO();

template Task<> Delegates::Bidirectional<SocketTag::BT>::send(std::string);
template Task<RecvResult> Delegates::Bidirectional<SocketTag::BT>::recv();
template void Delegates::Bidirectional<SocketTag::BT>::cancelIO();
#endif
