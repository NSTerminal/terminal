// Copyright 2021 the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#ifndef _WIN32
#include <bluetooth/bluetooth.h> // bdaddr_t
#endif

// Enum of all possible connection types
enum ConnectionType { TCP, UDP, Bluetooth };

// String representations of connection types
inline const char* connectionTypesStr[] = { "TCP", "UDP", "Bluetooth" };

/// <summary>
/// Structure containing metadata about a device (type, name, address, port) to provide information on how to connect to
/// it and how to describe it.
/// </summary>
struct DeviceData {
    int type; // Type of connection
    std::string name; // Name of device (Bluetooth only)
    std::string address; // Address of device (IP address for TCP/UDP, MAC address for Bluetooth)
    uint16_t port; // Port/channel of device
    uint64_t btAddr; // Bluetooth address as a 64-bit unsigned integer (used on Windows only)
};

/// <summary>
/// Namespace containing mutable values to configure the application.
/// </summary>
namespace Settings {
    inline uint8_t fontSize = 13; // Application font height in pixels
    inline uint8_t connectTimeout = 5; // Number of seconds to allow for connection before it aborts
    inline uint8_t connectPollTime = 100; // Milliseconds between poll checks while connecting
    inline bool showFPScounter = false; // If a framerate counter is shown in the top-right corner
}