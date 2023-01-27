// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#if OS_WINDOWS
#include "os/async_internal.hpp"

#include <WinSock2.h>

#include "os/async.hpp"
#include "os/errcheck.hpp"

static HANDLE completionPort = nullptr; // The IOCP handle

void Async::Internal::init(unsigned int numThreads) {
    // Start Winsock
    WSADATA wsaData{};
    call(FN(WSAStartup, MAKEWORD(2, 2), &wsaData), checkTrue, useReturnCode); // MAKEWORD(2, 2) for Winsock 2.2

    // Initialize IOCP
    completionPort = call(FN(CreateIoCompletionPort, INVALID_HANDLE_VALUE, nullptr, 0, numThreads), checkTrue);
}

void Async::Internal::stopThreads(unsigned int numThreads) {
    for (size_t i = 0; i < numThreads; i++) PostQueuedCompletionStatus(completionPort, 0, ASYNC_INTERRUPT, nullptr);
}

void Async::Internal::cleanup() {
    CloseHandle(completionPort);

    // Cleanup Winsock
    call(FN(WSACleanup));
}

Async::CompletionResult Async::Internal::worker() {
    DWORD numBytes;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped = nullptr;

    // Dequeue a completion packet from the system and check for the exit condition
    BOOL ret = GetQueuedCompletionStatus(completionPort, &numBytes, &completionKey, &overlapped, INFINITE);
    if (completionKey == ASYNC_INTERRUPT) throw WorkerInterruptedError{};

    // Get the structure with completion data, passed through the overlapped pointer
    // No locking is needed to modify the structure's fields - the calling coroutine will be suspended at this
    // point so mutually-exclusive access is guaranteed.
    auto& result = toResult(overlapped);
    result.res = static_cast<int>(numBytes);

    // Pass any failure back to the calling coroutine
    if (!ret) result.error = System::getLastError();

    return result;
}

void Async::add(SOCKET sockfd) {
    // Create a new handle for the socket
    call(FN(CreateIoCompletionPort, reinterpret_cast<HANDLE>(sockfd), completionPort, 0, 0), checkTrue);
}
#endif
