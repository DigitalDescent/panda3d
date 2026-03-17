#!/usr/bin/env python
"""
Steam Networking - Chat Client

Connects to a GNS chat server, displays incoming messages on screen, and lets
the user type messages via standard input (the console window).

Usage:
    python client.py [--host HOST] [--port PORT]

Defaults: host=127.0.0.1, port=27015
"""

import sys
import argparse
import threading

from direct.showbase.ShowBase import ShowBase
from direct.gui.OnscreenText import OnscreenText
from direct.task.TaskManagerGlobal import taskMgr

from panda3d.core import Datagram, DatagramIterator, TextNode

from panda3d.gns import (
    GNSConnectionManager,
    GNSConnectionReader,
    GNSConnectionWriter,
    GNSDatagram,
    GNSNetAddress,
)

# ── Message protocol (must match server.py) ────────────────────────────────
MSG_CHAT = 1
MSG_JOIN = 2
MSG_LEAVE = 3
MSG_SERVER_INFO = 4


class ChatClient(ShowBase):
    """Simple GNS chat client with an on-screen message log."""

    MAX_LINES = 18  # how many chat lines to show on screen

    def __init__(self, host, port):
        ShowBase.__init__(self)
        self.set_background_color(0.05, 0.05, 0.12, 1)
        self.disable_mouse()

        self.host = host
        self.port = port

        # On-screen UI
        self.title = OnscreenText(
            text="GNS Chat — connecting...",
            style=1, fg=(1, 1, 1, 1), shadow=(0, 0, 0, 1),
            pos=(0, 0.92), scale=0.07,
        )
        self.instructions = OnscreenText(
            text="Type in the console window and press Enter to chat.  Press Escape to quit.",
            parent=self.a2dBottomCenter, fg=(0.7, 0.7, 0.7, 1),
            pos=(0, 0.06), scale=0.05,
        )
        self.chat_lines = []
        self.chat_nodes = []

        self.accept("escape", sys.exit)

        # GNS networking
        self.manager = GNSConnectionManager()
        self.reader = GNSConnectionReader(self.manager, 0)
        self.writer = GNSConnectionWriter(self.manager, 0)
        self.connection = None

        self._connect(host, port)

        # Background thread reading lines from stdin
        self._input_lines = []
        self._input_lock = threading.Lock()
        input_thread = threading.Thread(target=self._stdin_reader, daemon=True)
        input_thread.start()

        self.taskMgr.add(self.update, "client-update")

    # ── Connection ─────────────────────────────────────────────────────────
    def _connect(self, host, port):
        self.connection = self.manager.open_connection_to_host(host, port)
        if self.connection is None or not self.connection.is_valid():
            print(f"ERROR: Could not open connection to {host}:{port}")
            sys.exit(1)
        print(f"[Client] Connecting to {host}:{port}...")

    # ── Per-frame update ───────────────────────────────────────────────────
    def update(self, task):
        self.manager.poll_callbacks()

        # Check for disconnect
        if self.manager.reset_connection_available():
            self.manager.get_reset_connection()
            self._add_chat_line("*** Disconnected from server ***", (1, 0.3, 0.3, 1))
            self.title.setText("GNS Chat — DISCONNECTED")
            self.connection = None
            return task.done

        # Read incoming messages
        self._read_messages()

        # Send any lines the user typed in the console
        self._flush_input()

        return task.cont

    # ── Receiving ──────────────────────────────────────────────────────────
    def _read_messages(self):
        while self.reader.data_available():
            datagram = GNSDatagram()
            if not self.reader.get_data(datagram):
                break

            iterator = DatagramIterator(datagram)
            msg_type = iterator.get_uint8()

            if msg_type == MSG_CHAT:
                nickname = iterator.get_string()
                text = iterator.get_string()
                self._add_chat_line(f"{nickname}: {text}", (0.9, 0.9, 0.9, 1))

            elif msg_type == MSG_SERVER_INFO:
                text = iterator.get_string()
                self._add_chat_line(text, (0.4, 1, 0.4, 1))
                self.title.setText("GNS Chat — CONNECTED")

            elif msg_type == MSG_JOIN:
                text = iterator.get_string()
                self._add_chat_line(text, (0.4, 0.8, 1, 1))

            elif msg_type == MSG_LEAVE:
                text = iterator.get_string()
                self._add_chat_line(text, (1, 0.8, 0.4, 1))

    # ── Sending ────────────────────────────────────────────────────────────
    def _flush_input(self):
        """Send any lines queued from the stdin reader thread."""
        with self._input_lock:
            lines = list(self._input_lines)
            self._input_lines.clear()

        for line in lines:
            if self.connection is None:
                break
            if not line.strip():
                continue
            dg = Datagram()
            dg.add_uint8(MSG_CHAT)
            dg.add_string(line.strip())
            self.writer.send(dg, self.connection)
            self._add_chat_line(f"(you): {line.strip()}", (0.6, 1, 0.6, 1))

    def _stdin_reader(self):
        """Background thread that reads lines from the console."""
        try:
            while True:
                line = input()
                with self._input_lock:
                    self._input_lines.append(line)
        except EOFError:
            pass

    # ── On-screen chat log ─────────────────────────────────────────────────
    def _add_chat_line(self, text, color=(1, 1, 1, 1)):
        print(text)
        self.chat_lines.append((text, color))
        if len(self.chat_lines) > self.MAX_LINES:
            self.chat_lines = self.chat_lines[-self.MAX_LINES:]
        self._refresh_chat_display()

    def _refresh_chat_display(self):
        for node in self.chat_nodes:
            node.destroy()
        self.chat_nodes.clear()

        for i, (text, color) in enumerate(self.chat_lines):
            y = 0.8 - i * 0.09
            node = OnscreenText(
                text=text, fg=color,
                parent=self.a2dTopLeft,
                pos=(0.08, y), scale=0.05,
                align=TextNode.ALeft,
                shadow=(0, 0, 0, 0.8),
            )
            self.chat_nodes.append(node)


def main():
    parser = argparse.ArgumentParser(description="GNS Chat Client")
    parser.add_argument("--host", default="127.0.0.1", help="Server address")
    parser.add_argument("--port", type=int, default=27015, help="Server port")
    args = parser.parse_args()

    app = ChatClient(args.host, args.port)
    app.run()


if __name__ == "__main__":
    main()
