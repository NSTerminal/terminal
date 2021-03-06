// Copyright 2021 the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A class to handle a socket connection in a GUI window

#pragma once

#include <string> // std::string
#include <functional> // std::bind()

#include "console.hpp"
#include "net/sockets.hpp"

/// <summary>
/// A class to handle a connection in an easy-to-use GUI.
/// </summary>
class ConnWindow {
    SOCKET _sockfd; // Socket for connections
    bool _connected = false; // If the window has an active connection
    short _pollFlags = POLLOUT; // What to check for when polling the socket

    std::string _title; // Title of window
    std::string _windowText; // The text in the window's title bar
    bool _open = true; // If the window is open (affected by the close button)
    Console _output; // Console output with send textbox

    /// <summary>
    /// Send a string through the socket if it's connected.
    /// </summary>
    /// <param name="s">The string to send</param>
    void _sendHandler(const std::string& s);

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
        _sockfd = INVALID_SOCKET;
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
    ConnWindow(const std::string& title, const std::string& extraInfo, SOCKET sockfd, int lastErr) : _sockfd(sockfd),
        _title(title),
        _windowText((extraInfo.empty()) ? _title : std::format("({}) {}", extraInfo, _title)),
        _output(std::bind(&ConnWindow::_sendHandler, this, std::placeholders::_1)) {
        // The internal Console object is intialized to send data when the textbox is submitted (i.e. Enter pressed)
        // This overloaded version of the constructor (taking a function which takes a `const std::string&`) enables
        // the textbox and its related config options.

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
    /// Get the window's title.
    /// </summary>
    /// <returns>The window's title</returns>
    std::string getTitle() {
        return _title;
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
    short getPollFlags() const {
        return _pollFlags;
    }

    /// <summary>
    /// Check if the window is open.
    /// </summary>
    bool open() const {
        return _open;
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
