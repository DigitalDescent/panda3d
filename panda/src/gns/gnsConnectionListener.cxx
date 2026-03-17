/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionListener.cxx
 * @author thetestgame
 * @date 2026-03-17
 */

#include "gnsConnectionListener.h"
#include "config_gns.h"

TypeHandle GNSConnectionListener::_type_handle;

/**
 *
 */
GNSConnectionListener::
GNSConnectionListener(GNSConnectionManager *manager) :
  _manager(manager)
{
}

/**
 *
 */
GNSConnectionListener::
~GNSConnectionListener() {
}

/**
 * Returns true if a new incoming GNS connection is available.
 */
bool GNSConnectionListener::
new_connection_available() const {
  return _manager->new_connection_available();
}

/**
 * If a new connection is available, fills in the connection object and remote
 * address and returns true.  Returns false otherwise.
 */
bool GNSConnectionListener::
get_new_connection(PT(GNSConnection) &new_connection,
                   GNSNetAddress &address) {
  PT(GNSConnection) listen_connection;
  return _manager->get_new_connection(listen_connection, address, new_connection);
}

/**
 *
 */
GNSConnectionManager *GNSConnectionListener::
get_manager() const {
  return _manager;
}
