/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file config_gns.h
 * @author thetestgame
 * @date 2026-03-17
 */

#ifndef CONFIG_GNS_H
#define CONFIG_GNS_H

#include "pandabase.h"
#include "notifyCategoryProxy.h"
#include "configVariableInt.h"
#include "configVariableDouble.h"
#include "configVariableBool.h"
#include "dconfig.h"

NotifyCategoryDecl(gns, EXPCL_PANDA_GNS, EXPTP_PANDA_GNS);

extern ConfigVariableInt gns_reader_threads;
extern ConfigVariableInt gns_writer_threads;
extern ConfigVariableInt gns_poll_timeout_ms;

extern EXPCL_PANDA_GNS void init_libgns();

#endif
