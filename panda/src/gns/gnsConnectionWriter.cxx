/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionWriter.cxx
 * @author thetestgame
 * @date 2026-03-17
 */

#include "gnsConnectionWriter.h"
#include "config_gns.h"
#include "lightMutexHolder.h"
#include "lightReMutexHolder.h"

TypeHandle GNSConnectionWriter::_type_handle;

// ---------------------------------------------------------------
// WriterThread
// ---------------------------------------------------------------

GNSConnectionWriter::WriterThread::
WriterThread(GNSConnectionWriter *writer, int thread_index) :
  Thread(std::string("GNSWriter") + std::to_string(thread_index),
         std::string("GNSWriter") + std::to_string(thread_index)),
  _writer(writer),
  _thread_index(thread_index)
{
}

void GNSConnectionWriter::WriterThread::
thread_main() {
  _writer->thread_run();
}

// ---------------------------------------------------------------
// GNSConnectionWriter
// ---------------------------------------------------------------

/**
 * Constructs a GNSConnectionWriter.  If num_threads is 0, all sends are
 * immediate (synchronous).  Otherwise, sends are queued for background
 * delivery.
 */
GNSConnectionWriter::
GNSConnectionWriter(GNSConnectionManager *manager, int num_threads) :
  _manager(manager),
  _shutdown(false),
  _immediate(num_threads == 0),
  _max_queue_size(10000),
  _queue_mutex("GNSConnectionWriter::_queue_mutex")
{
  nassertv(manager != nullptr);
  manager->add_writer(this);

  for (int i = 0; i < num_threads; i++) {
    PT(WriterThread) thread = new WriterThread(this, i);
    _threads.push_back(thread);
    thread->start(TP_normal, true);
  }
}

/**
 *
 */
GNSConnectionWriter::
~GNSConnectionWriter() {
  shutdown();
  if (_manager != nullptr) {
    _manager->remove_writer(this);
    _manager = nullptr;
  }
}

/**
 * Sends a datagram over the specified GNS connection.
 *
 * send_flags controls reliability/ordering:
 *   SF_reliable            - Reliable and ordered (default)
 *   SF_unreliable          - Unreliable, may arrive out of order
 *   SF_reliable_no_nagle   - Reliable, bypass Nagle
 *   SF_unreliable_no_nagle - Unreliable, bypass Nagle
 *   SF_no_delay            - Flush immediately
 *
 * If block is true and the queue is full, this will block until space is
 * available.  Only relevant when num_threads > 0.
 */
bool GNSConnectionWriter::
send(const Datagram &datagram, const PT(GNSConnection) &connection,
     int send_flags, bool block) {
  if (connection == nullptr || !connection->is_valid()) {
    return false;
  }

  if (_immediate) {
    return send_immediate(datagram, connection, send_flags);
  }

  QueuedSend qs;
  qs._datagram = datagram;
  qs._connection = connection;
  qs._send_flags = send_flags;

  LightMutexHolder holder(_queue_mutex);
  if (!block && (int)_queue.size() >= _max_queue_size) {
    gns_cat.warning()
      << "GNS writer queue full, dropping datagram\n";
    return false;
  }
  _queue.push_back(std::move(qs));
  return true;
}

/**
 * Returns the manager.
 */
GNSConnectionManager *GNSConnectionWriter::
get_manager() const {
  return _manager;
}

/**
 * Returns true if this writer sends immediately (no threads).
 */
bool GNSConnectionWriter::
is_immediate() const {
  return _immediate;
}

/**
 * Returns the number of writer threads.
 */
int GNSConnectionWriter::
get_num_threads() const {
  return (int)_threads.size();
}

/**
 * Sets the maximum queue size for queued sends.
 */
void GNSConnectionWriter::
set_max_queue_size(int max_size) {
  _max_queue_size = max_size;
}

/**
 * Returns the max queue size.
 */
int GNSConnectionWriter::
get_max_queue_size() const {
  return _max_queue_size;
}

/**
 * Returns the current number of queued sends.
 */
int GNSConnectionWriter::
get_current_queue_size() const {
  LightMutexHolder holder(((GNSConnectionWriter *)this)->_queue_mutex);
  return (int)_queue.size();
}

/**
 * Shuts down all writer threads.
 */
void GNSConnectionWriter::
shutdown() {
  _shutdown = true;
  for (auto &thread : _threads) {
    thread->join();
  }
  _threads.clear();
}

/**
 * Sends the datagram synchronously on the current thread.
 */
bool GNSConnectionWriter::
send_immediate(const Datagram &datagram, const PT(GNSConnection) &connection,
               int send_flags) {
  ISteamNetworkingSockets *iface = connection->get_interface();
  if (iface == nullptr) {
    return false;
  }

  const void *data = datagram.get_data();
  uint32_t size = (uint32_t)datagram.get_length();

  LightReMutexHolder holder(connection->get_write_mutex());
  EResult result = iface->SendMessageToConnection(
    connection->get_handle(), data, size, send_flags, nullptr);

  if (result != k_EResultOK) {
    gns_cat.error()
      << "GNS SendMessageToConnection failed: " << (int)result << "\n";
    return false;
  }

  return true;
}

/**
 * Background thread function: drains the queue and sends.
 */
void GNSConnectionWriter::
thread_run() {
  while (!_shutdown) {
    QueuedSend qs;
    bool have_data = false;

    {
      LightMutexHolder holder(_queue_mutex);
      if (!_queue.empty()) {
        qs = std::move(_queue.front());
        _queue.pop_front();
        have_data = true;
      }
    }

    if (have_data) {
      send_immediate(qs._datagram, qs._connection, qs._send_flags);
    } else {
      Thread::sleep(0.001);
    }
  }
}
