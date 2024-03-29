// Copyright 2021-2024 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

module;
#include <string>

#if OS_WINDOWS
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <netdb.h>
#endif

#include <ztd/out_ptr.hpp>

#include "os/check.hpp"

module net.netutils;
import net.enums;
import os.errcheck;
import os.error;
import utils.strings;

#if !OS_WINDOWS
constexpr auto GetAddrInfo = getaddrinfo;
constexpr auto GetNameInfo = getnameinfo;
#endif

AddrInfoHandle NetUtils::resolveAddr(const Device& device, bool useDNS) {
    bool isUDP = device.type == ConnectionType::UDP;
    AddrInfoType hints{
        .ai_flags = useDNS ? 0 : AI_NUMERICHOST,
        .ai_family = AF_UNSPEC,
        .ai_socktype = isUDP ? SOCK_DGRAM : SOCK_STREAM,
        .ai_protocol = isUDP ? IPPROTO_UDP : IPPROTO_TCP,
    };

    // Wide encoding conversions for Windows
    Strings::SysStr addrWide = Strings::toSys(device.address);
    Strings::SysStr portWide = Strings::toSys(device.port);

    // Resolve the IP
    AddrInfoHandle ret;
    CHECK(GetAddrInfo(addrWide.c_str(), portWide.c_str(), &hints, ztd::out_ptr::out_ptr(ret)), checkZero, useReturnCode,
        System::ErrorType::AddrInfo);

    return ret;
}

Device NetUtils::fromAddr(const sockaddr* addr, socklen_t addrLen, ConnectionType type) {
    constexpr auto nullChar = Strings::SysStr::value_type{};

    Strings::SysStr ipStr(NI_MAXHOST, nullChar);
    Strings::SysStr portStr(NI_MAXSERV, nullChar);

    auto ipLen = static_cast<socklen_t>(ipStr.size());
    auto portLen = static_cast<socklen_t>(portStr.size());

    CHECK(GetNameInfo(addr, addrLen, ipStr.data(), ipLen, portStr.data(), portLen, NI_NUMERICHOST | NI_NUMERICSERV),
        checkZero, useReturnCode, System::ErrorType::AddrInfo);

    // Process returned strings
    Strings::stripNull(ipStr);
    std::string ip = Strings::fromSys(ipStr);
    auto port = static_cast<uint16_t>(std::stoi(Strings::fromSys(portStr)));

    return { type, "", ip, port };
}

uint16_t NetUtils::getPort(Traits::SocketHandleType<SocketTag::IP> handle, bool isV4) {
    sockaddr_storage addr;
    socklen_t localAddrLen = sizeof(addr);
    CHECK(getsockname(handle, reinterpret_cast<sockaddr*>(&addr), &localAddrLen));

    uint16_t port
        = isV4 ? reinterpret_cast<sockaddr_in*>(&addr)->sin_port : reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port;
    return ntohs(port);
}

ServerAddress NetUtils::startServer(const Device& serverInfo, Delegates::SocketHandle<SocketTag::IP>& handle) {
    auto resolved = resolveAddr(serverInfo);
    bool isTCP = serverInfo.type == ConnectionType::TCP;
    bool isV4 = false;

    NetUtils::loopWithAddr(resolved.get(), [&handle, &isV4, isTCP](const AddrInfoType* result) {
        // Only AF_INET/AF_INET6 are supported
        switch (result->ai_family) {
            case AF_INET:
                isV4 = true;
                break;
            case AF_INET6:
                isV4 = false;
                break;
            default:
                return;
        }

        handle.reset(CHECK(socket(result->ai_family, result->ai_socktype, result->ai_protocol)));

        // Bind and listen
        CHECK(bind(*handle, result->ai_addr, static_cast<socklen_t>(result->ai_addrlen)));
        if (isTCP) CHECK(listen(*handle, SOMAXCONN));
    });

    return { getPort(*handle, isV4), isV4 ? IPType::IPv4 : IPType::IPv6 };
}
