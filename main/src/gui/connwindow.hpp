// Copyright 2021 the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A class to handle a socket connection in a GUI window

#pragma once

#include <string> // std::string

#include "console.hpp"
#include "net/sockets.hpp"

/// <summary>
/// A class to handle a connection in an easy-to-use GUI.
/// </summary>
class ConnWindow {
    const SOCKET _sockfd; // Socket for connections
    bool _connected = false; // If the window has an active connection
    bool _error = false; // If a fatal error occurred
    short _pollFlags = POLLOUT; // What to check for when polling the socket

    const std::string _title; // Title of window
    bool _open = true; // If the window is open (affected by the close button)
    Console _output; // Console output with send textbox

    /// <summary>
    /// Print a given error code (and its description), then close the socket.
    /// </summary>
    /// <param name="err">The error code to print</param>
    void _errorHandler(int err);

    /// <summary>
    /// Print the last error code and description, then close the socket.
    /// </summary>
    void _errorHandler() {
        _errorHandler(Sockets::getLastErr());
    }

    /// <summary>
    /// Close the internal socket.
    /// </summary>
    void _closeConnection() {
        Sockets::closeSocket(_sockfd);
        _connected = false;
        _pollFlags = 0;
    }

public:
    /// <summary>
    /// ConnWindow constructor, initialize a new object that can send/receive data across a socket file descriptor.
    /// </summary>
    /// <param name="title">The title of the window, shown in the title bar</param>
    /// <param name="sockfd">The socket file descriptor to use</param>
    /// <param name="lastErr">The error that occurred, taken right after the socket has been created</param>
    ConnWindow(const std::string& title, SOCKET sockfd, int lastErr) : _sockfd(sockfd), _title(title) {
        // Initialize the internal Console object to send data when the textbox is submitted (i.e. Enter pressed)
        // This overloaded version of the constructor (taking a function which takes a `const std::string&`) enables
        // the textbox and its related config options.
        // (This is not put in the initializer list because of its large size.)
        _output = Console([&](const std::string& s) {
            if (_connected) {
                // Connected, send data and check for errors
                int ret = Sockets::sendData(_sockfd, s);
                if (ret == SOCKET_ERROR) _errorHandler();
            } else {
                // Not connected, output a message
                _output.addInfo("The socket is not connected.");
            }
        });

        _output.addInfo("Connecting...");
        _errorHandler(lastErr); // Handle the error, this function does nothing if the operation was successful
    }

    /// <summary>
    /// ConnWindow destructor, close the socket file descriptor.
    /// </summary>
    ~ConnWindow() {
        _closeConnection();
    }

    /// <summary>
    /// Get the internal socket descriptor.
    /// </summary>
    /// <returns>The file descriptor of the managed socket</returns>
    SOCKET getSocket() const {
        return _sockfd;
    }

    /// <summary>
    /// Get the desired flags for polling this window's socket with (WSA)poll().
    /// </summary>
    /// <returns>The polling flags, set `pollfd::events` to this</returns>
    short getPollFlags() {
        return _pollFlags;
    }

    /// <summary>
    /// Check if the window is open.
    /// </summary>
    operator bool() const {
        return _open;
    }

    /// <summary>
    /// Compare this window's title to another given title.
    /// </summary>
    /// <param name="s">The title to compare with</param>
    /// <returns>If this window's title and the given title are equal</returns>
    bool operator==(const std::string& s) const {
        return _title == s;
    }

    /// <summary>
    /// Function callback to execute when the socket has a connect event.
    /// </summary>
    void connectHandler();

    /// <summary>
    /// Function callback to execute when the socket has an input or disconnect event.
    /// </summary>
    void inputHandler();

    /// <summary>
    /// Redraw the connection window and send data through the socket.
    /// </summary>
    void update();
};
