/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnection.h
 * @author thetestgame
 * @date 2026-03-17
 */

#ifndef GNSCONNECTION_H
#define GNSCONNECTION_H

#include "pandabase.h"
#include "typedReferenceCount.h"
#include "lightReMutex.h"
#include "gnsNetAddress.h"

#include <steam/steamnetworkingsockets.h>

class GNSConnectionManager;

/**
 * Represents a single GameNetworkingSockets connection, analogous to the
 * existing Connection class for TCP/UDP.  Each GNSConnection wraps an
 * HSteamNetConnection handle and maintains a reference to the ISteamNetworkingSockets
 * interface that owns it.
 *
 * GNS connections are reliable, ordered, and optionally unreliable, depending
 * on the send flags used by GNSConnectionWriter.
 */
class EXPCL_PANDA_GNS GNSConnection : public TypedReferenceCount {
PUBLISHED:
  ~GNSConnection();

  GNSNetAddress get_address() const;
  GNSConnectionManager *get_manager() const;

  HSteamNetConnection get_handle() const;

  bool is_valid() const;

  BLOCKING bool flush();

  void set_send_buffer_size(int size);

  void output(std::ostream &out) const;

public:
  GNSConnection(GNSConnectionManager *manager,
                ISteamNetworkingSockets *iface,
                HSteamNetConnection conn);

  ISteamNetworkingSockets *get_interface() const;
  LightReMutex &get_write_mutex();

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    TypedReferenceCount::init_type();
    register_type(_type_handle, "GNSConnection",
                  TypedReferenceCount::get_class_type());
  }
  virtual TypeHandle get_type() const {
    return get_class_type();
  }
  virtual TypeHandle force_init_type() {
    init_type();
    return get_class_type();
  }

private:
  GNSConnectionManager *_manager;
  ISteamNetworkingSockets *_interface;
  HSteamNetConnection _conn;
  LightReMutex _write_mutex;
  static TypeHandle _type_handle;
};

INLINE std::ostream &operator <<(std::ostream &out, const GNSConnection &conn) {
  conn.output(out);
  return out;
}

#endif
