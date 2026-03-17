#!/usr/bin/env python
"""
Steam Networking - Chat Server

A simple chat server using Panda3D's GNS (GameNetworkingSockets) transport
layer.  Clients connect and send chat messages; the server broadcasts each
message to every other connected client.

Run this script first, then launch one or more client.py instances.

Usage:
    python server.py [--port PORT]

Default port: 27015
"""

import sys
import argparse

from direct.showbase.ShowBase import ShowBase
from direct.task.TaskManagerGlobal import taskMgr

from panda3d.core import Datagram, DatagramIterator
from panda3d.gns import (
    GNSConnectionManager,
    GNSConnectionListener,
    GNSConnectionReader,
    GNSConnectionWriter,
    GNSDatagram,
)

# ── Message protocol ───────────────────────────────────────────────────────
# A single uint8 message type followed by payload.
MSG_CHAT = 1          # client→server or server→client: chat text
MSG_JOIN = 2          # server→client: a user joined
MSG_LEAVE = 3         # server→client: a user left
MSG_SERVER_INFO = 4   # server→client: welcome / id assignment


class ChatServer:
    """Simple broadcast chat server over GNS."""

    def __init__(self, port):
        self.port = port
        self.clients = {}  # GNSConnection → nickname string

        # Core GNS objects
        self.manager = GNSConnectionManager()
        self.listener = GNSConnectionListener(self.manager)
        self.reader = GNSConnectionReader(self.manager, 0)
        self.writer = GNSConnectionWriter(self.manager, 0)

        # Open a listen socket
        self.listen_conn = self.manager.open_listen_socket(port)
        if self.listen_conn is None or not self.listen_conn.is_valid():
            print(f"ERROR: Failed to open listen socket on port {port}")
            sys.exit(1)

        print(f"[Server] Listening on port {port}")

        # Register the per-frame task
        taskMgr.add(self.update, "server-update")

    # ── Per-frame update ───────────────────────────────────────────────────
    def update(self, task):
        # Drive GNS callbacks (connection state changes)
        self.manager.poll_callbacks()

        # Accept new connections
        self._check_new_connections()

        # Check for disconnected clients
        self._check_disconnections()

        # Read incoming datagrams
        self._read_messages()

        return task.cont

    # ── Connection handling ────────────────────────────────────────────────
    def _check_new_connections(self):
        while self.listener.new_connection_available():
            result = self.listener.get_new_connection()
            if result is None:
                continue
            new_conn, address = result

            nickname = address.get_ip_string()
            self.clients[new_conn] = nickname
            print(f"[Server] Client connected: {nickname}")

            # Send a welcome message
            dg = Datagram()
            dg.add_uint8(MSG_SERVER_INFO)
            dg.add_string(f"Welcome to the chat server! You are {nickname}")
            self.writer.send(dg, new_conn)

            # Notify everyone else
            self._broadcast_system(MSG_JOIN, f"{nickname} has joined.", exclude=new_conn)

    def _check_disconnections(self):
        while self.manager.reset_connection_available():
            result = self.manager.get_reset_connection()
            if result is None:
                continue
            conn = result

            nickname = self.clients.pop(conn, "???")
            print(f"[Server] Client disconnected: {nickname}")
            self._broadcast_system(MSG_LEAVE, f"{nickname} has left.")

    # ── Message reading ────────────────────────────────────────────────────
    def _read_messages(self):
        while self.reader.data_available():
            datagram = GNSDatagram()
            if not self.reader.get_data(datagram):
                break

            sender_conn = datagram.get_connection()
            iterator = DatagramIterator(datagram)

            msg_type = iterator.get_uint8()
            if msg_type == MSG_CHAT:
                text = iterator.get_string()
                nickname = self.clients.get(sender_conn, "???")
                print(f"[Chat] {nickname}: {text}")

                # Broadcast to all other clients
                self._broadcast_chat(nickname, text, exclude=sender_conn)

    # ── Broadcasting helpers ───────────────────────────────────────────────
    def _broadcast_chat(self, nickname, text, exclude=None):
        dg = Datagram()
        dg.add_uint8(MSG_CHAT)
        dg.add_string(nickname)
        dg.add_string(text)
        self._send_to_all(dg, exclude)

    def _broadcast_system(self, msg_type, text, exclude=None):
        dg = Datagram()
        dg.add_uint8(msg_type)
        dg.add_string(text)
        self._send_to_all(dg, exclude)

    def _send_to_all(self, datagram, exclude=None):
        for conn in self.clients:
            if conn is not exclude:
                self.writer.send(datagram, conn)


def main():
    parser = argparse.ArgumentParser(description="GNS Chat Server")
    parser.add_argument("--port", type=int, default=27015, help="Listen port")
    args = parser.parse_args()

    # Headless — no window needed for the server
    base = ShowBase(windowType="none")
    server = ChatServer(args.port)
    base.run()


if __name__ == "__main__":
    main()
