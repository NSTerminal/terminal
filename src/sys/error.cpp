// Copyright 2021-2022 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <cerrno> // errno
#endif

#include <magic_enum.hpp>

#include "error.hpp"
#include "util/formatcompat.hpp"

template <>
constexpr std::string_view magic_enum::customize::enum_name<System::ErrorType>(System::ErrorType value) noexcept {
    if (value == System::ErrorType::AddrInfo) return "getaddrinfo";
    return {};
}

std::string System::SystemError::formatted() const {
#ifdef _WIN32
    // Message buffer
    char msg[512];

    // Get the message text
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                   nullptr,
                   code,
                   LocaleNameToLCID(L"en-US", 0),
                   msg,
                   static_cast<DWORD>(std::ssize(msg)),
                   nullptr);

#else
    // strerrordesc_np (a GNU extension) is used since it doesn't translate the error message. A translation is
    // undesirable since the rest of the program isn't translated either.
    const char* msg = (type == ErrorType::System) ? strerrordesc_np(code) : gai_strerror(code);
#endif

    return std::format("{} (type {}, in {}): {}", code, magic_enum::enum_name(type), fnName, msg);
}

System::ErrorCode System::getLastError() {
#ifdef _WIN32
    return GetLastError();
#else
    return errno;
#endif
}

void System::setLastError(ErrorCode code) {
#ifdef _WIN32
    SetLastError(code);
#else
    errno = code;
#endif
}

bool System::isFatal(ErrorCode code) {
    // Check if the code is actually an error
    if (code == NO_ERROR) return false;

#ifdef _WIN32
    // This error means an operation hasn't failed, it's still waiting.
    // Tell the calling function that there's no error, and it should check back later.
    else if (code == WSA_IO_PENDING) return false;
#endif

    return true;
}