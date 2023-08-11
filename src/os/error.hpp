// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#if OS_WINDOWS
#include <WinSock2.h>
#else
// Error status codes
constexpr auto SOCKET_ERROR = -1; // An error has occurred (returned from a function)
constexpr auto NO_ERROR = 0;      // Done successfully (returned from a function)
#endif

namespace System {
#if OS_WINDOWS
    using ErrorCode = DWORD;
#else
    using ErrorCode = int;
#endif

    // Where an error came from.
    enum class ErrorType {
        System,      // From socket functions or other OS APIs
        AddrInfo,    // From a call to getaddrinfo
        IOReturn     // From a call to a macOS kernel function
    };

    // Gets the last error code. This is platform-specific.
    ErrorCode getLastError();

    // Checks if an error code should be handled as a fatal error.
    bool isFatal(ErrorCode code);

    // Formats a system error into a readable string.
    std::string formatSystemError(ErrorCode code, ErrorType type, std::string_view fnName);

    // An exception structure containing details of an error.
    struct SystemError : std::runtime_error {
        ErrorCode code = NO_ERROR;          // The platform-specific error code
        ErrorType type = ErrorType::System; // The type of the error
        std::string fnName;                 // The name of the function that caused the error

        // Constructs an object representing a specific error.
        SystemError(ErrorCode code, ErrorType type, std::string_view fnName) :
            std::runtime_error(formatSystemError(code, type, fnName)), code(code), type(type), fnName(fnName) {}

        // Checks if this object represents a fatal error.
        explicit operator bool() const {
            return isFatal(code);
        }

        // Checks if this exception represents a canceled operation.
        bool isCanceled() const;
    };
}
