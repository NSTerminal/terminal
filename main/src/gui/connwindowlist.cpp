// Copyright 2021 the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm> // std::replace()

#include "connwindowlist.hpp"
#include "util/formatcompat.hpp"

/// <summary>
/// Format a DeviceData into a string.
/// </summary>
/// <param name="data">The DeviceData to format</param>
/// <returns>The resultant string</returns>
/// <remarks>
/// This function is used to get a usable title+id for a ConnWindow using the passed DeviceData's variable fields.
/// </remarks>
static std::string formatDeviceData(const DeviceData& data) {
    // Type of the connection
    const char* type = Sockets::connectionTypesStr[data.type];
    bool isBluetooth = (data.type == Sockets::Bluetooth);

    // Bluetooth connections are described using the device's name (e.g. "MyESP32"),
    // IP connections (TCP/UDP) use the device's IP address (e.g. 192.168.0.178).
    std::string deviceString = (isBluetooth) ? data.name : data.address;

    // Newlines may be present in a Bluetooth device name, and if they get into a window's title, anything after the
    // first one will get cut off (the title bar can only hold one line). Replace them with spaces to keep everything on
    // one line.
    std::replace(deviceString.begin(), deviceString.end(), '\n', ' ');

    // Format the values into a string as the title
    // The address is always part of the id hash.
    // The port is not visible for a Bluetooth connection, instead, it is part of the id hash.
    const char* fmtStr = (isBluetooth) ? "{} Connection - {}##{} {}" : "{} Connection - {} port {}##{}";
    return std::format(fmtStr, type, deviceString, data.port, data.address);
}

void ConnWindowList::_populateFds() {
    _pfds.clear();
    for (const auto& window : _windows) _pfds.push_back({ window->getSocket(), POLLIN | POLLOUT, 0 });
}

bool ConnWindowList::add(const DeviceData& data) {
    // Title of the ConnWindow
    std::string title = formatDeviceData(data);

    // Check if the title already exists
    bool isNew = std::find_if(_windows.begin(), _windows.end(), [title](const ConnWindowPtr& current) {
        return *current == title;
    }) == _windows.end();

    // Add the window to the list
    if (isNew) {
        _windows.push_back(std::make_unique<ConnWindow>(title, _connectFunction(data)));
        _populateFds();
    }
    return isNew;
}

void ConnWindowList::update() {
    for (size_t i = 0; i < _windows.size(); i++) {
        auto& current = _windows[i];
        if (*current) {
            current->update(); // Window is open, update it
        } else {
            _windows.erase(_windows.begin() + i); // Window is closed, remove it from vector
            _populateFds(); // Update file descriptor poll vector
            break; // Iterators are invalid, wait until the next iteration
        }
    }

    assert(_windows.size() == _pfds.size());
    Sockets::poll(_pfds, 0);

    for (size_t i = 0; i < _pfds.size(); i++) {
        auto revents = _pfds[i].revents;
        auto& current = _windows[i];

        if ((revents & POLLIN) || (revents & POLLHUP)) current->inputHandler();
        if (revents & POLLOUT) current->connectHandler();
        if (revents & POLLERR) current->errorHandler();
    }
}