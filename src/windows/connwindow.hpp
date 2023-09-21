// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "consolewindow.hpp"

#include "net/device.hpp"
#include "sockets/socket.hpp"

// Handles a socket connection in a GUI window.
class ConnWindow : public ConsoleWindow {
    friend class ConnServerWindow;

    std::unique_ptr<Socket> socket; // Internal socket
    Device device;
    std::atomic_bool connected = false;
    std::atomic_bool pendingRecv = false;
    unsigned int recvSize = 1024; // Unsigned int to work with ImGuiDataType

    // Connects to the server.
    Task<> connect();

    // Sends a string through the socket.
    Task<> sendHandlerAsync(std::string s);

    // Receives a string from the socket and displays it in the console output.
    // The receive size is passed as a parameter to avoid concurrent access.
    Task<> readHandler(unsigned int size);

    // Connects to the target server.
    void onInit() override {
        connect();
    }

    void sendHandler(std::string_view s) override {
        sendHandlerAsync(std::string{ s });
    }

    void drawOptions() override;

    // Handles incoming I/O.
    void onBeforeUpdate() override;

    void onUpdate() override {
        updateConsole();
    }

public:
    // Sets the window information (title and remote host).
    ConnWindow(std::unique_ptr<Socket>&& socket, const Device& device, std::string_view extraInfo);

    // Cancels pending socket I/O.
    ~ConnWindow() override {
        socket->cancelIO();
    }
};
