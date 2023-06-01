# Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# This script is intended to support testing of Network Socket Terminal.

import argparse
import json
import pathlib
import socket

HOST = '::'

# Signals a request for the server to echo back what the client sent
ECHO_STRING = b'echo test'

# Command-line arguments to set server configuration
parser = argparse.ArgumentParser()
parser.add_argument('-t', '--transport', type=str, required=True)

args = parser.parse_args()
is_tcp = args.transport == 'TCP'

socket_type = socket.SOCK_STREAM if is_tcp else socket.SOCK_DGRAM

# Load settings from JSON
SETTINGS_FILE = pathlib.Path(__file__).parent.parent / 'settings' / 'settings.json'

with open(SETTINGS_FILE) as f:
    data = json.load(f)

    PORT = data['ip']['tcpPort'] if is_tcp else data['ip']['udpPort']


# Handles I/O for a TCP server.
def server_loop_tcp(s: socket.socket):
    # Wait for new client connections
    conn, addr = s.accept()
    peer_addr = addr[0]

    print(f'New connection from {peer_addr}')

    with conn:
        while True:
            # Wait for new data to be received
            data = conn.recv(1024)
            if data:
                print(f'Received: {data.decode()}')

                # Send back data if requested
                if data == ECHO_STRING:
                    conn.sendall(data)
            else:
                # Connection closed by client
                print(f'Disconnected from {peer_addr}')
                conn.close()
                break


# Handles I/O for a UDP server.
def server_loop_udp(s: socket.socket):
    data, addr = s.recvfrom(1024)
    if data:
        print(f'Received: {data} (from {addr[0]})')

        if data == ECHO_STRING:
            s.sendto(data, addr)


with socket.socket(socket.AF_INET6, socket_type) as s:
    # Enable dual-stack server so IPv4 and IPv6 clients can connect
    s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
    s.bind((HOST, PORT))

    if is_tcp:
        s.listen()

    print(f'Server is active on port {PORT}')

    # Handle clients until termination
    while True:
        try:
            if is_tcp:
                server_loop_tcp(s)
            else:
                server_loop_udp(s)
        except IOError as e:
            print('I/O Error:', e)
            break
        except KeyboardInterrupt as e:
            print('Interrupted')
            break
