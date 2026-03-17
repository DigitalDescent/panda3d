/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionReader.h
 * @author thetestgame
 * @date 2026-03-17
 */

#ifndef GNSCONNECTIONREADER_H
#define GNSCONNECTIONREADER_H

#include "pandabase.h"
#include "typedReferenceCount.h"
#include "datagram.h"
#include "pointerTo.h"
#include "lightMutex.h"
#include "pdeque.h"
#include "thread.h"
#include "pvector.h"
#include "gnsConnectionManager.h"
#include "gnsConnection.h"
#include "gnsNetAddress.h"

/**
 * A datagram that carries the originating GNS connection and remote address.
 */
class EXPCL_PANDA_GNS GNSDatagram : public Datagram {
PUBLISHED:
  GNSDatagram();
  GNSDatagram(const void *data, size_t size);

  void set_connection(const PT(GNSConnection) &connection);
  PT(GNSConnection) get_connection() const;

  void set_address(const GNSNetAddress &address);
  const GNSNetAddress &get_address() const;

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    Datagram::init_type();
    register_type(_type_handle, "GNSDatagram",
                  Datagram::get_class_type());
  }
  virtual TypeHandle get_type() const {
    return get_class_type();
  }
  virtual TypeHandle force_init_type() {
    init_type();
    return get_class_type();
  }

private:
  PT(GNSConnection) _connection;
  GNSNetAddress _address;
  static TypeHandle _type_handle;
};

/**
 * Reads datagrams from GameNetworkingSockets connections using a poll group.
 * This is analogous to QueuedConnectionReader for TCP/UDP.
 *
 * The reader polls the GNS poll group for incoming messages and queues
 * them as GNSDatagram objects for retrieval.
 *
 * If num_threads is 0 (default), use poll() to drive reading from the
 * application's main loop.  If num_threads > 0, reader threads are spawned
 * that poll continuously.
 */
class EXPCL_PANDA_GNS GNSConnectionReader : public TypedReferenceCount {
PUBLISHED:
  GNSConnectionReader(GNSConnectionManager *manager, int num_threads = 0);
  virtual ~GNSConnectionReader();

  bool data_available() const;
  bool get_data(GNSDatagram &result);

  void poll();

  bool is_polling() const;
  int get_num_threads() const;

  GNSConnectionManager *get_manager() const;

  void set_max_queue_size(int max_size);
  int get_max_queue_size() const;

  void shutdown();

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    TypedReferenceCount::init_type();
    register_type(_type_handle, "GNSConnectionReader",
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
  void thread_run();
  void do_poll();

  GNSConnectionManager *_manager;
  bool _shutdown;
  bool _polling;
  int _max_queue_size;

  pdeque<GNSDatagram> _queue;
  mutable LightMutex _queue_mutex;

  class ReaderThread : public Thread {
  public:
    ReaderThread(GNSConnectionReader *reader, int thread_index);
    virtual void thread_main();

    GNSConnectionReader *_reader;
    int _thread_index;
  };

  typedef pvector<PT(ReaderThread)> Threads;
  Threads _threads;

  static TypeHandle _type_handle;
};

#endif
