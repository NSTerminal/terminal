// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sockets/delegates.hpp"
#include "sockets/traits/client.hpp"
#include "sockets/traits/sockethandle.hpp"
#include "utils/task.hpp"

namespace Delegates {
    // Manages operations on client sockets.
    template <auto Tag>
    class Client : public ClientDelegate {
        using Handle = Traits::SocketHandleType<Tag>;

        const Handle& _handle;
        const Traits::Client<Tag>& _traits;

    public:
        Client(const Handle& handle, const Traits::Client<Tag>& traits) : _handle(handle), _traits(traits) {}

        Task<> connect() const override;

        const Device& getServer() const override {
            return _traits.device;
        }
    };
}