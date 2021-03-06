// Copyright 2021 the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A custom container to store and manage multiple ConnWindow objects

#pragma once

#include <string>
#include <memory>
#include <functional>

#include "connwindow.hpp"
#include "net/sockets.hpp"

/// <summary>
/// A class to manage multiple ConnWindow objects.
/// </summary>
class ConnWindowList {
    using PtrType = std::unique_ptr<ConnWindow>; // A unique_ptr pointing to a managed ConnWindow object
    using VectorType = std::vector<PtrType>; // A vector of the above type

    std::function<SOCKET(Sockets::DeviceData)> _connectFunction; // The function to call when creating a new window
    VectorType _windows; // All window pointers and their corresponding titles
    std::vector<pollfd> _pfds; // Vector of socket fds to poll
    int _pollRet = NO_ERROR; // The result of Sockets::poll()

    /// <summary>
    /// Overwrite the list of poll sockets with the managed ConnWindow objects' socket descriptors.
    /// </summary>
    void _populateFds();

public:
    /// <summary>
    /// ConnWindowList constructor, set the connect function to use.
    /// </summary>
    /// <typeparam name="Fn">A function which takes a DeviceData and returns a SOCKET</typeparam>
    /// <param name="fn">The function to use as the connect function when creating a new window</param>
    template <class Fn>
    ConnWindowList(Fn&& fn) : _connectFunction(fn) {}

    /// <summary>
    /// Add a new, unique window to the list.
    /// </summary>
    /// <param name="data">The DeviceData to initialize the window with</param>
    /// <param name="extraInfo">A string prepended to the window title in parentheses (optional)</param>
    /// <returns>If the window is unique and added</returns>
    bool add(const Sockets::DeviceData& data, const std::string& extraInfo = "");

    /// <summary>
    /// Redraw all contained windows and delete any that have been closed.
    /// </summary>
    /// <returns>NO_ERROR if socket polling was successful, SOCKET_ERROR if it failed</returns>
    int update();
};
