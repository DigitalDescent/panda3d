/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionManager.h
 * @author thetestgame
 * @date 2026-03-17
 */

#ifndef GNSCONNECTIONMANAGER_H
#define GNSCONNECTIONMANAGER_H

#include "pandabase.h"
#include "typedReferenceCount.h"
#include "pointerTo.h"
#include "pset.h"
#include "pvector.h"
#include "lightMutex.h"
#include "gnsConnection.h"
#include "gnsNetAddress.h"
#include "pdeque.h"

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifdef GNS_USE_STEAMWORKS
#include <steam/steam_api.h>
#endif

class GNSConnectionReader;
class GNSConnectionWriter;

/**
 * The primary interface for creating and managing GameNetworkingSockets
 * connections.  This is analogous to ConnectionManager for TCP/UDP.
 *
 * The manager owns the ISteamNetworkingSockets interface and a set of poll
 * groups.  It initialises the GNS library on construction and shuts it down
 * on destruction (unless the application manages the library lifetime
 * externally).
 *
 * Typical usage:
 * @code
 *   PT(GNSConnectionManager) mgr = new GNSConnectionManager;
 *   PT(GNSConnection) server = mgr->open_listen_socket(27015);
 *   // add server to a GNSConnectionListener
 *
 *   PT(GNSConnection) client = mgr->open_connection_to_host("1.2.3.4", 27015);
 *   // add client to a GNSConnectionReader + GNSConnectionWriter
 * @endcode
 */
class EXPCL_PANDA_GNS GNSConnectionManager : public TypedReferenceCount {
PUBLISHED:
  GNSConnectionManager();
  virtual ~GNSConnectionManager();

  PT(GNSConnection) open_connection_to_host(const std::string &hostname,
                                            uint16_t port);
  PT(GNSConnection) open_connection_to_host(const GNSNetAddress &address);

  PT(GNSConnection) open_listen_socket(uint16_t port);
  PT(GNSConnection) open_listen_socket(const GNSNetAddress &address);

  bool close_connection(const PT(GNSConnection) &connection,
                        int reason = 0,
                        const std::string &debug_msg = std::string());

  void poll_callbacks();

  bool reset_connection_available() const;
  bool get_reset_connection(PT(GNSConnection) &connection);

  bool new_connection_available() const;
  bool get_new_connection(PT(GNSConnection) &listen_connection,
                          GNSNetAddress &address,
                          PT(GNSConnection) &new_connection);

  size_t get_num_connections() const;

public:
  ISteamNetworkingSockets *get_interface() const;
  HSteamNetPollGroup get_poll_group() const;

  void add_reader(GNSConnectionReader *reader);
  void remove_reader(GNSConnectionReader *reader);
  void add_writer(GNSConnectionWriter *writer);
  void remove_writer(GNSConnectionWriter *writer);

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    TypedReferenceCount::init_type();
    register_type(_type_handle, "GNSConnectionManager",
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
  static void steam_net_connection_status_changed(
    SteamNetConnectionStatusChangedCallback_t *info);
  void on_connection_status_changed(
    SteamNetConnectionStatusChangedCallback_t *info);

public:
  PT(GNSConnection) find_connection(HSteamNetConnection conn) const;

private:
  ISteamNetworkingSockets *_interface;
  HSteamListenSocket _listen_socket;
  HSteamNetPollGroup _poll_group;

  bool _owns_library;

  typedef phash_set<PT(GNSConnection)> Connections;
  Connections _connections;

  typedef phash_set<GNSConnectionReader *, pointer_hash> Readers;
  typedef phash_set<GNSConnectionWriter *, pointer_hash> Writers;
  Readers _readers;
  Writers _writers;

  // Queued events for polling
  struct NewConnectionData {
    PT(GNSConnection) _listen_connection;
    GNSNetAddress _address;
    PT(GNSConnection) _new_connection;
  };

  pdeque<NewConnectionData> _new_connections;
  pdeque<PT(GNSConnection)> _reset_connections;

  LightMutex _set_mutex;

  static GNSConnectionManager *_active_manager;
  static TypeHandle _type_handle;
};

#endif
