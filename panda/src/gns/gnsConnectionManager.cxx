/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionManager.cxx
 * @author thetestgame
 * @date 2026-03-17
 */

#include "gnsConnectionManager.h"
#include "gnsConnectionReader.h"
#include "gnsConnectionWriter.h"
#include "config_gns.h"
#include "lightMutexHolder.h"

TypeHandle GNSConnectionManager::_type_handle;
GNSConnectionManager *GNSConnectionManager::_active_manager = nullptr;

/**
 * Debug output callback for the GNS library.
 */
static void gns_debug_output(ESteamNetworkingSocketsDebugOutputType type,
                              const char *msg) {
  switch (type) {
  case k_ESteamNetworkingSocketsDebugOutputType_Error:
    gns_cat.error() << msg << "\n";
    break;
  case k_ESteamNetworkingSocketsDebugOutputType_Warning:
    gns_cat.warning() << msg << "\n";
    break;
  case k_ESteamNetworkingSocketsDebugOutputType_Msg:
    gns_cat.info() << msg << "\n";
    break;
  default:
    gns_cat.debug() << msg << "\n";
    break;
  }
}

/**
 * Constructs a GNSConnectionManager, initialising the GNS library.
 */
GNSConnectionManager::
GNSConnectionManager() :
  _interface(nullptr),
  _listen_socket(k_HSteamListenSocket_Invalid),
  _poll_group(k_HSteamNetPollGroup_Invalid),
  _owns_library(false),
  _set_mutex("GNSConnectionManager::_set_mutex")
{
  // Initialize the networking library.
#ifdef GNS_USE_STEAMWORKS
  // When building against the Steamworks SDK, Steam manages the GNS
  // lifecycle.  SteamAPI_Init() must have been called by the application
  // before constructing a GNSConnectionManager.
  if (SteamNetworkingSockets() == nullptr) {
    gns_cat.error()
      << "SteamAPI has not been initialized.  Call SteamAPI_Init() "
         "before creating a GNSConnectionManager.\n";
    return;
  }
  _owns_library = false;
#else
  // Standalone open-source build: we own init/shutdown.
  SteamDatagramErrMsg err_msg;
  if (!GameNetworkingSockets_Init(nullptr, err_msg)) {
    gns_cat.error()
      << "GameNetworkingSockets_Init failed: " << err_msg << "\n";
    return;
  }
  _owns_library = true;
#endif

  // Hook up debug output.
  SteamNetworkingUtils()->SetDebugOutputFunction(
    k_ESteamNetworkingSocketsDebugOutputType_Msg, gns_debug_output);

  _interface = SteamNetworkingSockets();
  if (_interface == nullptr) {
    gns_cat.error()
      << "Failed to obtain ISteamNetworkingSockets interface\n";
    return;
  }

  // Create a poll group that all connections will join.
  _poll_group = _interface->CreatePollGroup();
  if (_poll_group == k_HSteamNetPollGroup_Invalid) {
    gns_cat.error()
      << "Failed to create GNS poll group\n";
  }

  _active_manager = this;

  gns_cat.info()
    << "GNSConnectionManager initialized\n";
}

/**
 *
 */
GNSConnectionManager::
~GNSConnectionManager() {
  // Close all connections.
  {
    LightMutexHolder holder(_set_mutex);
    for (auto &conn : _connections) {
      if (conn->is_valid()) {
        _interface->CloseConnection(conn->get_handle(), 0, nullptr, false);
      }
    }
    _connections.clear();
  }

  if (_listen_socket != k_HSteamListenSocket_Invalid) {
    _interface->CloseListenSocket(_listen_socket);
    _listen_socket = k_HSteamListenSocket_Invalid;
  }

  if (_poll_group != k_HSteamNetPollGroup_Invalid) {
    _interface->DestroyPollGroup(_poll_group);
    _poll_group = k_HSteamNetPollGroup_Invalid;
  }

  if (_active_manager == this) {
    _active_manager = nullptr;
  }

  if (_owns_library) {
#ifndef GNS_USE_STEAMWORKS
    GameNetworkingSockets_Kill();
#endif
    _owns_library = false;
  }

  gns_cat.info()
    << "GNSConnectionManager destroyed\n";
}

/**
 * Opens a client connection to a remote host.  Returns null on failure.
 */
PT(GNSConnection) GNSConnectionManager::
open_connection_to_host(const std::string &hostname, uint16_t port) {
  GNSNetAddress addr;
  addr.set_host(hostname, port);
  return open_connection_to_host(addr);
}

/**
 * Opens a client connection to a remote host given a GNSNetAddress.
 */
PT(GNSConnection) GNSConnectionManager::
open_connection_to_host(const GNSNetAddress &address) {
  if (_interface == nullptr) {
    gns_cat.error()
      << "GNS interface not initialized\n";
    return nullptr;
  }

  SteamNetworkingConfigValue_t opt;
  opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
             (void *)steam_net_connection_status_changed);

  HSteamNetConnection conn = _interface->ConnectByIPAddress(
    address.get_addr(), 1, &opt);

  if (conn == k_HSteamNetConnection_Invalid) {
    gns_cat.error()
      << "Failed to connect to " << address << "\n";
    return nullptr;
  }

  if (!_interface->SetConnectionPollGroup(conn, _poll_group)) {
    gns_cat.error()
      << "Failed to assign connection to poll group\n";
    _interface->CloseConnection(conn, 0, nullptr, false);
    return nullptr;
  }

  PT(GNSConnection) connection = new GNSConnection(this, _interface, conn);

  {
    LightMutexHolder holder(_set_mutex);
    _connections.insert(connection);
  }

  gns_cat.info()
    << "Opened GNS connection to " << address << " (handle " << conn << ")\n";
  return connection;
}

/**
 * Opens a listen socket on the given port on all interfaces.  Returns a
 * pseudo-connection representing the listen socket; add it to a
 * GNSConnectionListener to accept incoming connections.
 */
PT(GNSConnection) GNSConnectionManager::
open_listen_socket(uint16_t port) {
  GNSNetAddress addr;
  addr.set_any(port);
  return open_listen_socket(addr);
}

/**
 * Opens a listen socket on the given address.  Returns a pseudo-connection
 * representing the listen socket.
 */
PT(GNSConnection) GNSConnectionManager::
open_listen_socket(const GNSNetAddress &address) {
  if (_interface == nullptr) {
    gns_cat.error()
      << "GNS interface not initialized\n";
    return nullptr;
  }

  SteamNetworkingConfigValue_t opt;
  opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
             (void *)steam_net_connection_status_changed);

  HSteamListenSocket listen = _interface->CreateListenSocketIP(
    address.get_addr(), 1, &opt);

  if (listen == k_HSteamListenSocket_Invalid) {
    gns_cat.error()
      << "Failed to listen on " << address << "\n";
    return nullptr;
  }

  _listen_socket = listen;

  // Create a pseudo-connection to represent the listen socket for user code.
  PT(GNSConnection) connection =
    new GNSConnection(this, _interface, k_HSteamNetConnection_Invalid);

  {
    LightMutexHolder holder(_set_mutex);
    _connections.insert(connection);
  }

  gns_cat.info()
    << "Listening for GNS connections on " << address << "\n";
  return connection;
}

/**
 * Closes a connection.
 */
bool GNSConnectionManager::
close_connection(const PT(GNSConnection) &connection, int reason,
                 const std::string &debug_msg) {
  if (connection == nullptr || !connection->is_valid()) {
    return false;
  }

  const char *msg = debug_msg.empty() ? nullptr : debug_msg.c_str();
  _interface->CloseConnection(connection->get_handle(), reason, msg, false);

  LightMutexHolder holder(_set_mutex);
  _connections.erase(connection);
  return true;
}

/**
 * Runs GNS callbacks to process connection status changes.  This should be
 * called regularly (e.g., once per frame) to dispatch all pending events.
 */
void GNSConnectionManager::
poll_callbacks() {
  if (_interface != nullptr) {
    _interface->RunCallbacks();
  }
}

/**
 * Returns true if a reset (closed) connection event is available.
 */
bool GNSConnectionManager::
reset_connection_available() const {
  LightMutexHolder holder(((GNSConnectionManager *)this)->_set_mutex);
  return !_reset_connections.empty();
}

/**
 * Dequeues a reset connection event.
 */
bool GNSConnectionManager::
get_reset_connection(PT(GNSConnection) &connection) {
  LightMutexHolder holder(_set_mutex);
  if (_reset_connections.empty()) {
    return false;
  }
  connection = _reset_connections.front();
  _reset_connections.pop_front();
  return true;
}

/**
 * Returns true if a new incoming connection is available.
 */
bool GNSConnectionManager::
new_connection_available() const {
  LightMutexHolder holder(((GNSConnectionManager *)this)->_set_mutex);
  return !_new_connections.empty();
}

/**
 * Dequeues a new incoming connection event.
 */
bool GNSConnectionManager::
get_new_connection(PT(GNSConnection) &listen_connection,
                   GNSNetAddress &address,
                   PT(GNSConnection) &new_connection) {
  LightMutexHolder holder(_set_mutex);
  if (_new_connections.empty()) {
    return false;
  }
  NewConnectionData &data = _new_connections.front();
  listen_connection = data._listen_connection;
  address = data._address;
  new_connection = data._new_connection;
  _new_connections.pop_front();
  return true;
}

/**
 * Returns the number of tracked connections.
 */
size_t GNSConnectionManager::
get_num_connections() const {
  LightMutexHolder holder(((GNSConnectionManager *)this)->_set_mutex);
  return _connections.size();
}

/**
 * Returns the GNS interface pointer.
 */
ISteamNetworkingSockets *GNSConnectionManager::
get_interface() const {
  return _interface;
}

/**
 * Returns the poll group handle.
 */
HSteamNetPollGroup GNSConnectionManager::
get_poll_group() const {
  return _poll_group;
}

/**
 *
 */
void GNSConnectionManager::
add_reader(GNSConnectionReader *reader) {
  LightMutexHolder holder(_set_mutex);
  _readers.insert(reader);
}

/**
 *
 */
void GNSConnectionManager::
remove_reader(GNSConnectionReader *reader) {
  LightMutexHolder holder(_set_mutex);
  _readers.erase(reader);
}

/**
 *
 */
void GNSConnectionManager::
add_writer(GNSConnectionWriter *writer) {
  LightMutexHolder holder(_set_mutex);
  _writers.insert(writer);
}

/**
 *
 */
void GNSConnectionManager::
remove_writer(GNSConnectionWriter *writer) {
  LightMutexHolder holder(_set_mutex);
  _writers.erase(writer);
}

/**
 * Static callback dispatched by the GNS library when any connection's status
 * changes.  Routes to the active manager instance.
 */
void GNSConnectionManager::
steam_net_connection_status_changed(
  SteamNetConnectionStatusChangedCallback_t *info) {
  if (_active_manager != nullptr) {
    _active_manager->on_connection_status_changed(info);
  }
}

/**
 * Handles a connection status change event.
 */
void GNSConnectionManager::
on_connection_status_changed(
  SteamNetConnectionStatusChangedCallback_t *info) {

  switch (info->m_info.m_eState) {
  case k_ESteamNetworkingConnectionState_Connecting:
    {
      // An incoming connection is attempting to connect.
      if (info->m_eOldState == k_ESteamNetworkingConnectionState_None) {
        // Accept the connection.
        if (_interface->AcceptConnection(info->m_hConn) != k_EResultOK) {
          gns_cat.error()
            << "Failed to accept incoming GNS connection\n";
          _interface->CloseConnection(info->m_hConn, 0, nullptr, false);
          break;
        }

        if (!_interface->SetConnectionPollGroup(info->m_hConn, _poll_group)) {
          gns_cat.error()
            << "Failed to assign incoming connection to poll group\n";
          _interface->CloseConnection(info->m_hConn, 0, nullptr, false);
          break;
        }

        PT(GNSConnection) new_conn =
          new GNSConnection(this, _interface, info->m_hConn);

        GNSNetAddress addr(info->m_info.m_addrRemote);

        NewConnectionData data;
        data._listen_connection = nullptr;
        data._address = addr;
        data._new_connection = new_conn;

        {
          LightMutexHolder holder(_set_mutex);
          _connections.insert(new_conn);
          _new_connections.push_back(data);
        }

        gns_cat.info()
          << "Incoming GNS connection from " << addr
          << " (handle " << info->m_hConn << ")\n";
      }
    }
    break;

  case k_ESteamNetworkingConnectionState_Connected:
    {
      gns_cat.info()
        << "GNS connection " << info->m_hConn << " established\n";
    }
    break;

  case k_ESteamNetworkingConnectionState_ClosedByPeer:
  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
    {
      if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected ||
          info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
        gns_cat.info()
          << "GNS connection " << info->m_hConn << " lost: "
          << info->m_info.m_szEndDebug << "\n";

        PT(GNSConnection) conn = find_connection(info->m_hConn);
        if (conn != nullptr) {
          LightMutexHolder holder(_set_mutex);
          _reset_connections.push_back(conn);
          _connections.erase(conn);
        }
      }

      _interface->CloseConnection(info->m_hConn, 0, nullptr, false);
    }
    break;

  default:
    break;
  }
}

/**
 * Finds the GNSConnection object for the given handle, or returns null.
 */
PT(GNSConnection) GNSConnectionManager::
find_connection(HSteamNetConnection conn) const {
  LightMutexHolder holder(((GNSConnectionManager *)this)->_set_mutex);
  for (auto &c : _connections) {
    if (c->get_handle() == conn) {
      return c;
    }
  }
  return nullptr;
}
