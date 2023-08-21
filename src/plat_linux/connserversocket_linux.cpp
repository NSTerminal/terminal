// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#if OS_LINUX
#include "os/errcheck.hpp"
#include "sockets/connserversocket.hpp"
#include "sockets/enums.hpp"

template <>
void ConnServerSocket<SocketTag::BT>::_init(uint16_t port, int backlog) {
    //
}
#endif
