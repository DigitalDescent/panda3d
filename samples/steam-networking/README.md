# Steam Networking Sample

A minimal **chat server + client** demonstrating Panda3D's
`panda3d.gns` module — the GameNetworkingSockets (GNS) transport layer
created by Valve.

GNS provides reliable and unreliable messaging over UDP with built-in
encryption, connection management, and congestion control.

## Files

| File | Description |
|------|-------------|
| `server.py` | Headless chat server — accepts connections, relays messages |
| `client.py` | Windowed chat client — connects, shows messages on screen |

## Running

1. **Start the server** in one terminal:

```
python server.py
```

2. **Start one or more clients** in separate terminals:

```
python client.py
```

Type messages in the client's console window and press Enter to send.
Press Escape to quit the client.

### Options

Both scripts accept `--port PORT` (default `27015`).  The client also
accepts `--host HOST` (default `127.0.0.1`).

## What This Demonstrates

- **GNSConnectionManager** — initializes the GNS library and manages
  the connection lifecycle.
- **GNSConnectionListener** — polls for new incoming connections on
  the server side.
- **GNSConnectionReader** — receives incoming datagrams (in polling
  mode with `num_threads=0`).
- **GNSConnectionWriter** — sends datagrams reliably.
- **GNSDatagram** — extended `Datagram` that carries the originating
  connection and remote address.
- **Datagram / DatagramIterator** — Panda3D's standard binary
  serialization used for the message protocol.

## Message Protocol

Each message starts with a `uint8` type tag:

| Type | Value | Direction | Payload |
|------|-------|-----------|---------|
| `MSG_CHAT` | 1 | both | `string` (nickname, if from server) + `string` (text) |
| `MSG_JOIN` | 2 | server→client | `string` (system message) |
| `MSG_LEAVE` | 3 | server→client | `string` (system message) |
| `MSG_SERVER_INFO` | 4 | server→client | `string` (welcome text) |

## Requirements

Panda3D must be built with GNS support enabled (`--use-gns` for
makepanda, or `HAVE_GAMENETWORKINGSOCKETS` for CMake).
