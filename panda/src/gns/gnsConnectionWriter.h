/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionWriter.h
 * @author thetestgame
 * @date 2026-03-17
 */

#ifndef GNSCONNECTIONWRITER_H
#define GNSCONNECTIONWRITER_H

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

/**
 * Sends datagrams over GameNetworkingSockets connections, analogous to
 * ConnectionWriter for TCP/UDP.
 *
 * Supports both reliable and unreliable send modes.  If num_threads is 0,
 * sends are immediate (synchronous).  If num_threads > 0, sends are queued
 * and dispatched from background threads.
 */
class EXPCL_PANDA_GNS GNSConnectionWriter : public TypedReferenceCount {
PUBLISHED:
  /**
   * Send reliability mode.
   */
  enum SendFlags {
    SF_reliable            = k_nSteamNetworkingSend_Reliable,
    SF_unreliable          = k_nSteamNetworkingSend_Unreliable,
    SF_no_nagle            = k_nSteamNetworkingSend_NoNagle,
    SF_unreliable_no_nagle = k_nSteamNetworkingSend_UnreliableNoNagle,
    SF_no_delay            = k_nSteamNetworkingSend_NoDelay,
    SF_reliable_no_nagle   = k_nSteamNetworkingSend_ReliableNoNagle,
  };

  GNSConnectionWriter(GNSConnectionManager *manager, int num_threads = 0);
  virtual ~GNSConnectionWriter();

  BLOCKING bool send(const Datagram &datagram,
                     const PT(GNSConnection) &connection,
                     int send_flags = SF_reliable,
                     bool block = false);

  GNSConnectionManager *get_manager() const;
  bool is_immediate() const;
  int get_num_threads() const;

  void set_max_queue_size(int max_size);
  int get_max_queue_size() const;
  int get_current_queue_size() const;

  void shutdown();

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    TypedReferenceCount::init_type();
    register_type(_type_handle, "GNSConnectionWriter",
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
  bool send_immediate(const Datagram &datagram,
                      const PT(GNSConnection) &connection,
                      int send_flags);

  struct QueuedSend {
    Datagram _datagram;
    PT(GNSConnection) _connection;
    int _send_flags;
  };

  void thread_run();

  GNSConnectionManager *_manager;
  bool _shutdown;
  bool _immediate;
  int _max_queue_size;

  pdeque<QueuedSend> _queue;
  LightMutex _queue_mutex;

  class WriterThread : public Thread {
  public:
    WriterThread(GNSConnectionWriter *writer, int thread_index);
    virtual void thread_main();

    GNSConnectionWriter *_writer;
    int _thread_index;
  };

  typedef pvector<PT(WriterThread)> Threads;
  Threads _threads;

  static TypeHandle _type_handle;
};

#endif
