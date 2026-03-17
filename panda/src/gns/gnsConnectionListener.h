/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionListener.h
 * @author thetestgame
 * @date 2026-03-17
 */

#ifndef GNSCONNECTIONLISTENER_H
#define GNSCONNECTIONLISTENER_H

#include "pandabase.h"
#include "typedReferenceCount.h"
#include "gnsConnectionManager.h"
#include "gnsConnection.h"
#include "gnsNetAddress.h"

/**
 * Provides a polling interface for new incoming GNS connections, analogous
 * to QueuedConnectionListener for TCP.
 *
 * Incoming connections are detected via GNSConnectionManager::poll_callbacks()
 * and can be retrieved here.  This is a thin wrapper that delegates to the
 * manager's new-connection queue.
 *
 * Usage:
 * @code
 *   PT(GNSConnectionManager) mgr = new GNSConnectionManager;
 *   PT(GNSConnection) listen = mgr->open_listen_socket(27015);
 *   PT(GNSConnectionListener) listener = new GNSConnectionListener(mgr);
 *
 *   // Per frame:
 *   mgr->poll_callbacks();
 *   while (listener->new_connection_available()) {
 *     PT(GNSConnection) new_conn;
 *     GNSNetAddress addr;
 *     listener->get_new_connection(new_conn, addr);
 *     // ... use new_conn
 *   }
 * @endcode
 */
class EXPCL_PANDA_GNS GNSConnectionListener : public TypedReferenceCount {
PUBLISHED:
  explicit GNSConnectionListener(GNSConnectionManager *manager);
  virtual ~GNSConnectionListener();

  bool new_connection_available() const;
  bool get_new_connection(PT(GNSConnection) &new_connection,
                          GNSNetAddress &address);

  GNSConnectionManager *get_manager() const;

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    TypedReferenceCount::init_type();
    register_type(_type_handle, "GNSConnectionListener",
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
  static TypeHandle _type_handle;
};

#endif
