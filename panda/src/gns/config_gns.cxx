/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file config_gns.cxx
 * @author thetestgame
 * @date 2026-03-17
 */

#include "config_gns.h"
#include "gnsConnectionManager.h"
#include "gnsConnection.h"
#include "gnsConnectionReader.h"
#include "gnsConnectionWriter.h"
#include "gnsConnectionListener.h"
#include "gnsNetAddress.h"
#include "dconfig.h"

#if !defined(CPPPARSER) && !defined(LINK_ALL_STATIC) && !defined(BUILDING_PANDA_GNS)
  #error Apologies -- you must #define BUILDING_PANDA_GNS when compiling this module.
#endif

Configure(config_gns);
NotifyCategoryDef(gns, "");

ConfigureFn(config_gns) {
  init_libgns();
}

ConfigVariableInt gns_reader_threads
("gns-reader-threads", 1,
 PRC_DESC("The number of threads to devote to reading GNS datagrams."));

ConfigVariableInt gns_writer_threads
("gns-writer-threads", 0,
 PRC_DESC("The number of threads to devote to writing GNS datagrams.  "
          "Use 0 for immediate (synchronous) sends."));

ConfigVariableInt gns_poll_timeout_ms
("gns-poll-timeout-ms", 5,
 PRC_DESC("The timeout in milliseconds when polling GNS for incoming data."));

void
init_libgns() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  GNSConnectionManager::init_type();
  GNSConnection::init_type();
  GNSConnectionReader::init_type();
  GNSConnectionWriter::init_type();
  GNSConnectionListener::init_type();
  GNSNetAddress::init_type();
}
