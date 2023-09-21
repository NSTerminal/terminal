// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "socket.hpp"

#include "delegates/bidirectional.hpp"
#include "delegates/client.hpp"
#include "delegates/noops.hpp"
#include "delegates/sockethandle.hpp"
#include "net/enums.hpp"
#include "utils/task.hpp"

// A socket representing an outgoing connection.
template <auto Tag>
class ClientSocket : public Socket {
    Delegates::SocketHandle<Tag> handle;
    Delegates::Bidirectional<Tag> io{ handle };
    Delegates::Client<Tag> client;
    Delegates::NoopServer server;

public:
    // Constructs an object with a target device.
    ClientSocket() : Socket(handle, io, client, server), client(handle) {}
};

using ClientSocketIP = ClientSocket<SocketTag::IP>;
using ClientSocketBT = ClientSocket<SocketTag::BT>;
