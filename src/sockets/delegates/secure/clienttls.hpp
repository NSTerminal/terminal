// Copyright 2021-2023 Aidan Sun and the WhaleConnect contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <queue>
#include <string>

#include <botan/tls_alert.h>
#include <botan/tls_client.h>

#include "net/device.hpp"
#include "net/enums.hpp"
#include "sockets/delegates/bidirectional.hpp"
#include "sockets/delegates/client.hpp"
#include "sockets/delegates/delegates.hpp"
#include "sockets/delegates/sockethandle.hpp"
#include "utils/task.hpp"

namespace Delegates {
    // Manages operations on TLS client sockets.
    class ClientTLS : public HandleDelegate, public ClientDelegate, public IODelegate {
        std::unique_ptr<Botan::TLS::Client> channel;
        SocketHandle<SocketTag::IP> handle;
        Client<SocketTag::IP> baseClient{ handle };
        Bidirectional<SocketTag::IP> baseIO{ handle };

        std::queue<RecvResult> completedReads;
        std::queue<std::string> pendingWrites;

        // Sends all encrypted TLS data over the socket.
        Task<> sendQueued();

    public:
        ~ClientTLS() {
            close();
        }

        // Receives raw TLS data and passes it to the internal channel.
        Task<bool> recvBase(std::size_t size);

        void queueRead(std::string data) {
            completedReads.push({ true, false, data, std::nullopt });
        }

        void setAlert(Botan::TLS::Alert newAlert) {
            TLSAlert alert{ newAlert.type_string(), newAlert.is_fatal() };

            if (completedReads.empty()) completedReads.push({ true, false, "", alert });
            else completedReads.back().alert = alert;
        }

        void queueWrite(std::string data) {
            pendingWrites.push(data);
        }

        void close() override;

        bool isValid() override {
            return handle.isValid() && channel->is_active();
        }

        void cancelIO() override {
            handle.cancelIO();
        }

        Task<> connect(Device device) override;

        Task<> send(std::string data) override;

        Task<RecvResult> recv(std::size_t size) override;
    };
}
