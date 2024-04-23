// Copyright 2021-2024 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "components/windowlist.hpp"
#include "net/device.hpp"

// Adds a ConnWindow to a window list and handles errors during socket creation.
void addConnWindow(WindowList& list, bool useTLS, const Device& device, std::string_view extraInfo);

// Draws the new connection window.
void drawNewConnectionWindow(bool& open, WindowList& connections, WindowList& sdpWindows);
