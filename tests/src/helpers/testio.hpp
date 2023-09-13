// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "net/device.hpp"
#include "sockets/socket.hpp"

// Performs basic I/O checks on a socket.
void testIO(const Socket& socket, bool useRunLoop = false);

// Connects a socket, then performs I/O checks.
void testIOClient(const Socket& socket, const Device& device, bool useRunLoop = false);
