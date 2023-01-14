// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Settings {
    inline unsigned char fontSize = 13;     // Application font height in pixels
    inline bool useDefaultFont = false;     // If Dear ImGui's default font is used (instead of GNU Unifont)
    inline bool roundedCorners = false;     // If corners in the UI are rounded
    inline bool windowTransparency = false; // If application windows have a transparent effect
    inline unsigned char numThreads = 0;    // Number of worker threads (0 to auto-detect)
}
