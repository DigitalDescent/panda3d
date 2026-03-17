/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsNetAddress.h
 * @author thetestgame
 * @date 2026-03-17
 */

#ifndef GNSNETADDRESS_H
#define GNSNETADDRESS_H

#include "pandabase.h"
#include "typedReferenceCount.h"

#include <steam/steamnetworkingsockets.h>

/**
 * Wraps an SteamNetworkingIPAddr for use with the GNS transport layer.
 * Provides a Panda-native interface analogous to NetAddress.
 */
class EXPCL_PANDA_GNS GNSNetAddress : public TypedReferenceCount {
PUBLISHED:
  GNSNetAddress();
  GNSNetAddress(const GNSNetAddress &copy);
  GNSNetAddress(const SteamNetworkingIPAddr &addr);

  void set_host(const std::string &hostname, uint16_t port);
  void set_any(uint16_t port);

  std::string get_ip_string() const;
  uint16_t get_port() const;

  bool is_any() const;
  void clear();

  void output(std::ostream &out) const;

  bool operator ==(const GNSNetAddress &other) const;
  bool operator !=(const GNSNetAddress &other) const;
  bool operator <(const GNSNetAddress &other) const;

public:
  const SteamNetworkingIPAddr &get_addr() const;
  void set_addr(const SteamNetworkingIPAddr &addr);

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    TypedReferenceCount::init_type();
    register_type(_type_handle, "GNSNetAddress",
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
  SteamNetworkingIPAddr _addr;
  static TypeHandle _type_handle;
};

INLINE std::ostream &operator <<(std::ostream &out, const GNSNetAddress &addr) {
  addr.output(out);
  return out;
}

#endif
