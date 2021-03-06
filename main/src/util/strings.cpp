// Copyright 2021 the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef _WIN32
#include <Windows.h>
#endif

#include "strings.hpp"

Strings::widestr Strings::toWide(const std::string& from) {
#ifdef _WIN32
    // Nothing to convert in an empty string
    if (from.empty()) return {};

    // Size of UTF-8 string in UTF-16 wide encoding
    int stringSize = MultiByteToWideChar(CP_UTF8, 0, from.c_str(), static_cast<int>(from.size()), 0, 0);

    // Buffer to contain new string
    std::wstring buf(stringSize, '\0');

    // Convert the string from UTF-8 and return the converted buffer
    MultiByteToWideChar(CP_UTF8, 0, from.c_str(), -1, buf.data(), stringSize);
    return buf;
#else
    return from;
#endif
}

std::string Strings::fromWide(const Strings::widestr& from) {
#ifdef _WIN32
    // Nothing to convert in an empty string
    if (from.empty()) return {};

    // Size of UTF-16 wide string in UTF-8 encoding
    int stringSize = WideCharToMultiByte(CP_UTF8, 0, from.c_str(), static_cast<int>(from.size()), 0, 0, 0, 0);

    // Buffer to contain new string
    std::string buf(stringSize, '\0');

    // Convert the string to UTF-8 and return the converted buffer
    WideCharToMultiByte(CP_UTF8, 0, from.c_str(), -1, buf.data(), stringSize, nullptr, nullptr);
    return buf;
#else
    return from;
#endif
}

std::string Strings::replaceAll(NO_CONST_REF std::string str, const std::string& from, const std::string& to) {
    // Preliminary checks
    // 1. Nothing to replace in an empty string
    // 2. Can't replace an empty string
    // 3. If from and to are equal the function call becomes pointless
    if (str.empty() || from.empty() || (from == to)) return str;

    size_t start = 0;
    while ((start = str.find(from, start)) != std::string::npos) {
        str.replace(start, from.size(), to);
        start += to.size(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }

    return str;
}

std::vector<std::string> Strings::split(NO_CONST_REF std::string str, char delim) {
    // Return value
    std::vector<std::string> ret;

    // Find each delimited substring in the buffer to extract each individual substring
    size_t pos = 0;
    while ((pos = str.find(delim)) != std::string::npos) {
        pos++; // Increment position to include the delimiter in the substring
        ret.push_back(str.substr(0, pos)); // Get the substring, add it to the vector
        str.erase(0, pos); // Erase the substring from the buffer
    }

    // Add the last substring (all other substrings have been erased from the buffer, the only one left is the one after
    // the last delimiter char, or the whole string if there are no \n's present)
    ret.push_back(str);
    return ret;
}
