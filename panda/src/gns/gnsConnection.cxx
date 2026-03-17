/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnection.cxx
 * @author thetestgame
 * @date 2026-03-17
 */

#include "gnsConnection.h"
#include "gnsConnectionManager.h"
#include "config_gns.h"

TypeHandle GNSConnection::_type_handle;

/**
 * Creates a GNSConnection wrapping the given GNS connection handle.  This
 * should normally be called only by GNSConnectionManager.
 */
GNSConnection::
GNSConnection(GNSConnectionManager *manager,
              ISteamNetworkingSockets *iface,
              HSteamNetConnection conn) :
  _manager(manager),
  _interface(iface),
  _conn(conn)
{
}

/**
 *
 */
GNSConnection::
~GNSConnection() {
  if (_conn != k_HSteamNetConnection_Invalid && _interface != nullptr) {
    gns_cat.info()
      << "Closing GNS connection " << _conn << "\n";
    _interface->CloseConnection(_conn, 0, nullptr, false);
    _conn = k_HSteamNetConnection_Invalid;
  }
}

/**
 * Returns the remote address of this connection.
 */
GNSNetAddress GNSConnection::
get_address() const {
  if (_conn == k_HSteamNetConnection_Invalid || _interface == nullptr) {
    return GNSNetAddress();
  }

  SteamNetConnectionInfo_t info;
  if (_interface->GetConnectionInfo(_conn, &info)) {
    return GNSNetAddress(info.m_addrRemote);
  }
  return GNSNetAddress();
}

/**
 * Returns the manager that owns this connection.
 */
GNSConnectionManager *GNSConnection::
get_manager() const {
  return _manager;
}

/**
 * Returns the underlying GNS connection handle.
 */
HSteamNetConnection GNSConnection::
get_handle() const {
  return _conn;
}

/**
 * Returns true if this connection is still valid (not yet closed).
 */
bool GNSConnection::
is_valid() const {
  return _conn != k_HSteamNetConnection_Invalid;
}

/**
 * Flushes any pending outgoing messages on this connection.
 */
bool GNSConnection::
flush() {
  if (_conn == k_HSteamNetConnection_Invalid || _interface == nullptr) {
    return false;
  }
  EResult result = _interface->FlushMessagesOnConnection(_conn);
  return result == k_EResultOK;
}

/**
 * Sets the send buffer size for this connection.
 */
void GNSConnection::
set_send_buffer_size(int size) {
  if (_conn == k_HSteamNetConnection_Invalid || _interface == nullptr) {
    return;
  }
  SteamNetworkingConfigValue_t opt;
  opt.SetInt32(k_ESteamNetworkingConfig_SendBufferSize, size);
  _interface->SetConnectionOptions(_conn, &opt, 1);
}

/**
 * Returns the ISteamNetworkingSockets interface for direct API calls.
 */
ISteamNetworkingSockets *GNSConnection::
get_interface() const {
  return _interface;
}

/**
 * Returns the write mutex for thread-safe send operations.
 */
LightReMutex &GNSConnection::
get_write_mutex() {
  return _write_mutex;
}

/**
 *
 */
void GNSConnection::
output(std::ostream &out) const {
  out << "GNSConnection(" << _conn << ")";
}
