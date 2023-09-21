// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "window.hpp"

class WindowList {
    std::vector<std::unique_ptr<Window>> windows; // List of window pointers

    // Checks if the list contains a window with the specified title.
    bool validateDuplicate(std::string_view title) {
        auto windowIter
            = std::ranges::find_if(windows, [title](const auto& current) { return current->getTitle() == title; });

        return windowIter == windows.end();
    }

public:
    // Adds a new window to the list.
    template <class T, class... Args>
    requires std::derived_from<T, Window>
    auto add(Args&&... args) {
        // Create the pointer
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        if (!validateDuplicate(ptr->getTitle())) return false;

        // Add it to the list
        ptr->init();
        windows.push_back(std::move(ptr));
        return true;
    }

    // Redraws all contained windows and deletes any that have been closed.
    void update() {
        // Remove all closed windows
        std::erase_if(windows, [](const auto& window) { return !window->isOpen(); });

        // Update all open windows
        for (const auto& i : windows) i->update();
    }

    // Expose functions from internal vector

    auto begin() {
        return windows.begin();
    }

    auto end() {
        return windows.end();
    }

    bool empty() const {
        return windows.empty();
    }
};
