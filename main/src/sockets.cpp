// Copyright 2021 the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono> // std::chrono

#ifdef _WIN32
// Winsock headers for Windows
#include <ws2tcpip.h>
#include <ws2bth.h>

#pragma comment(lib, "Ws2_32.lib")

// These don't exist on Windows
#define EAI_SYSTEM 0
#define MSG_NOSIGNAL 0
#else
// Bluetooth headers for Linux
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include <cerrno> // errno
#include <cstring> // std::strerror()

#include <netinet/in.h> // sockaddr, sockaddr_in, sockaddr_in6
#include <sys/socket.h> // Socket definitions
#include <netdb.h> // addrinfo/getaddrinfo() related indentifiers
#include <unistd.h> // close()
#include <fcntl.h> // fcntl()
#include <poll.h> // poll()

// Windows API functions remapped to Berkeley Sockets API functions
#define WSAPoll poll
#define GetAddrInfoW getaddrinfo
#define FreeAddrInfoW freeaddrinfo

// Socket errors
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAETIMEDOUT ETIMEDOUT

// Bluetooth definitions
#define AF_BTH AF_BLUETOOTH
#define BTHPROTO_RFCOMM BTPROTO_RFCOMM

typedef addrinfo ADDRINFOW;
#endif

#include "sockets.hpp"
#include "error.hpp"
#include "winutf8.hpp"

int Sockets::getLastErr() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void Sockets::setLastErr(int err) {
#ifdef _WIN32
    WSASetLastError(err);
#else
    errno = err;
#endif
}

/// <summary>
/// Set the blocking status of a socket.
/// </summary>
/// <param name="sockfd">The socket file descriptor for which to set the blocking state</param>
/// <param name="blocking">If the socket blocks</param>
/// <returns>The return value of the function called internally [Windows: ioctlsocket(), Unix: fcntl()]</returns>
static int setBlocking(SOCKET sockfd, bool blocking) {
#ifdef _WIN32
    ULONG mode = !blocking;
    return ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == SOCKET_ERROR) return SOCKET_ERROR; // If flag read failed, abort
    flags = (blocking) ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(sockfd, F_SETFL, flags);
#endif
}

int Sockets::init() {
#ifdef _WIN32
    // Start Winsock on Windows
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData); // MAKEWORD(2, 2) for Winsock 2.2
#else
    return NO_ERROR;
#endif
}

void Sockets::cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

int Sockets::connectWithTimeout(SOCKET sockfd, const std::atomic<bool>& sig, sockaddr* addr, size_t addrlen) {
    // Make the socket non-blocking, abort if this action fails.
    if (setBlocking(sockfd, false) == SOCKET_ERROR) return SOCKET_ERROR;

    // Start connecting on the non-blocking socket
    connect(sockfd, addr, static_cast<int>(addrlen));
    bool success = false;

    // Get the last socket error
    int lastErr = getLastErr();
    if ((lastErr != WSAEWOULDBLOCK) && (lastErr != WSAEINPROGRESS)) {
        // Check if the last socket error is not (WSA)EWOULDBLOCK or (WSA)EINPROGRESS, these are non-fatal errors that
        // may be thrown with a non-blocking socket. If it's anything else, set return code to indicate failure.
        return SOCKET_ERROR;
    } else {
        setLastErr(NO_ERROR);
        auto start = std::chrono::steady_clock::now(); // When the loop started

        // We're not using (WSA)poll() directly because we need to periodically check for the signal.
        // (WSA)poll() blocks for the entirety of its execution, preventing the thread from checking anything else.
        while (!sig) {
            // Set up array of pollfd structures (we only need one here)
            pollfd pfds[] = { { sockfd, POLLOUT, 0 } };

            // Poll for a short amount of time
            if (WSAPoll(pfds, 1, Settings::connectPollTime) > 0) {
                // Return value greater than 0, this means the socket was connected successfully so break
                success = true;
                break;
            }

            // Check if the timeout value has been reached
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(Settings::connectTimeout)) break;
        }

        // If the connect was not successful, set the last error to be "Timed Out" and exit
        if (!success) {
            setLastErr(WSAETIMEDOUT);
            return SOCKET_ERROR;
        }
    }

    // Make socket blocking again
    return setBlocking(sockfd, true);
}

SOCKET Sockets::createClientSocket(const DeviceData& data, const std::atomic<bool>& sig) {
    SOCKET sockfd = INVALID_SOCKET; // Socket file descriptor
    bool success = false; // If the connection was successful

    if (data.type == Bluetooth) {
        // Bluetooth connection - set up socket
        sockfd = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        if (sockfd == INVALID_SOCKET) return INVALID_SOCKET;

        // Set up server address structure
#ifdef _WIN32
        SOCKADDR_BTH addr{
            .addressFamily = AF_BTH,
            .btAddr = data.btAddr,
            .serviceClassId = RFCOMM_PROTOCOL_UUID,
            .port = data.port
        };
#else
        sockaddr_rc addr{
            .rc_family = AF_BLUETOOTH,
            .rc_bdaddr = data.btAddr,
            .rc_channel = static_cast<uint8_t>(data.port)
        };
#endif

        // Connect
        success = connectWithTimeout(sockfd, sig, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR;
    } else {
        // Internet protocol - Set up hints for getaddrinfo()
        // Ignore "missing initializer" warnings on GCC and clang
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
        ADDRINFOW hints{
            .ai_flags = AI_NUMERICHOST,
            .ai_family = AF_UNSPEC,
            .ai_socktype = (data.type == TCP) ? SOCK_STREAM : SOCK_DGRAM,
            .ai_protocol = (data.type == TCP) ? IPPROTO_TCP : IPPROTO_UDP
        };
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif

        ADDRINFOW* addr;

        // uint16_t=>char[] conversion with snprintf()
        constexpr int strLen = 6;
        char portStr[strLen];
        std::snprintf(portStr, strLen, "%hu", data.port);

        // Wide encoding conversions for Windows
        // These are stored in their own variables to prevent them from being temporaries and destroyed later.
        // If we were to call `.c_str()` and then have these destroyed, `.c_str()` would be a dangling pointer.
        widestr addrWide = toWide(data.address.c_str());
        widestr portWide = toWide(portStr);

        // Resolve and connect to the IP, getaddrinfo() and GetAddrInfoW() allow both IPv4 and IPv6 addresses
        int gaiResult = GetAddrInfoW(addrWide.c_str(), portWide.c_str(), &hints, &addr);
        if (gaiResult == NO_ERROR) {
            // getaddrinfo() succeeded, initialize socket file descriptor with values created by GAI
            sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

            if (sockfd != INVALID_SOCKET) {
                // TCP may need a timeout, UDP does not
                if (data.type == TCP)
                    success = connectWithTimeout(sockfd, sig, addr->ai_addr, addr->ai_addrlen) != SOCKET_ERROR;
                else
                    success = connect(sockfd, addr->ai_addr, static_cast<int>(addr->ai_addrlen)) != SOCKET_ERROR;

                // Release the resources
                FreeAddrInfoW(addr);
            }
        } else {
            // Because we're using our own implementation of strerror() and getaddrinfo() errors differ from standard
            // ones (on Unix they're negative while errnos are all positive, and on Windows all the errors are in one
            // category and guaranteed not to clash) we can directly set errno to the result of getaddrinfo() and let
            // the application handle the error.
            if (gaiResult != EAI_SYSTEM) setLastErr(gaiResult);
        }
    }

    // Check if connection failed
    if (!success) {
        // Destroy the socket:
        destroySocket(sockfd);
        sockfd = INVALID_SOCKET;
    }

    return sockfd;
}

void Sockets::destroySocket(SOCKET sockfd) {
    // This may reset the last error code to 0, save it first:
    int lastErrBackup = getLastErr();

    if (sockfd != INVALID_SOCKET) {
#ifdef _WIN32
        shutdown(sockfd, SD_BOTH);
        closesocket(sockfd);
#else
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
#endif
    }

    // Restore the last error code:
    setLastErr(lastErrBackup);
}

int Sockets::sendData(SOCKET sockfd, const std::string& data) {
    // Send data through socket, static_cast the string size to avoid MSVC warning C4267
    // ('var': conversion from 'size_t' to 'type', possible loss of data)
    return send(sockfd, data.c_str(), static_cast<int>(data.size()), MSG_NOSIGNAL);

    // Note: Typically, sendto() and recvfrom() are used with a UDP connection.
    // However, these require a sockaddr parameter, which becomes hard to get with getaddrinfo().
    // Without this parameter, the call becomes:
    // sendto(sockfd, data, len, 0, nullptr, 0); (nullptr replaces the sockaddr param)
    // Which is equal to send(sockfd, data, len, 0); (https://linux.die.net/man/2/sendto)
    // And is the same function call for TCP/RFCOMM, also removing the need for an if statement.
    // With send() or without the sockaddr parameter, the socket requires a valid connection.
    // This is why connect() is used with UDP.
}

int Sockets::recvData(SOCKET sockfd, std::string& data) {
    constexpr int bufLen = 1024;
    char buf[bufLen] = "";

    // Receive and check bytes received
    int ret = recv(sockfd, buf, bufLen - 1, MSG_NOSIGNAL);
    if (ret != SOCKET_ERROR) {
        buf[ret] = '\0'; // Add a null char to the end of the buffer
        data = buf;
    }
    return ret;
}
