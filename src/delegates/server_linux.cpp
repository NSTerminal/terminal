// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#if OS_LINUX
#include <bit>
#include <functional>
#include <memory>

#include <liburing.h>
#include <netdb.h>
#include <netinet/in.h>

#include "server.hpp"

#include "delegates/delegates.hpp"
#include "net/enums.hpp"
#include "net/netutils.hpp"
#include "os/async_linux.hpp"
#include "os/errcheck.hpp"
#include "sockets/incomingsocket.hpp"

static void startAccept(int s, sockaddr* clientAddr, socklen_t& clientLen, Async::CompletionResult& result) {
    io_uring_sqe* sqe = Async::getUringSQE();
    io_uring_prep_accept(sqe, s, clientAddr, &clientLen, 0);
    io_uring_sqe_set_data(sqe, &result);
    Async::submitRing();
}

template <>
ServerAddress Delegates::Server<SocketTag::IP>::startServer(uint16_t port) {
    return NetUtils::startServer(port, _handle, _type, _traits.ip);
}

template <>
ServerAddress Delegates::Server<SocketTag::BT>::startServer(uint16_t port) {
    // TODO
    return {};
}

template <>
Task<AcceptResult> Delegates::Server<SocketTag::IP>::accept() {
    sockaddr_storage client;
    auto clientAddr = std::bit_cast<sockaddr*>(&client);
    socklen_t clientLen = sizeof(client);

    auto acceptResult = co_await Async::run(std::bind_front(startAccept, *_handle, clientAddr, clientLen));

    Device device = NetUtils::fromAddr(clientAddr, clientLen, ConnectionType::TCP);
    SocketHandle<SocketTag::IP> fd{ acceptResult.res };

    co_return { device, std::make_unique<IncomingSocket<SocketTag::IP>>(std::move(fd)) };
}

template <>
Task<AcceptResult> Delegates::Server<SocketTag::BT>::accept() {
    // TODO
    co_return {};
}
#endif
