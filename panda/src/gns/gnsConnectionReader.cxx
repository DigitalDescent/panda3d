/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsConnectionReader.cxx
 * @author thetestgame
 * @date 2026-03-17
 */

#include "gnsConnectionReader.h"
#include "config_gns.h"
#include "lightMutexHolder.h"

TypeHandle GNSDatagram::_type_handle;
TypeHandle GNSConnectionReader::_type_handle;

// ---------------------------------------------------------------
// GNSDatagram
// ---------------------------------------------------------------

/**
 *
 */
GNSDatagram::
GNSDatagram() {
}

/**
 *
 */
GNSDatagram::
GNSDatagram(const void *data, size_t size) :
  Datagram(data, size)
{
}

/**
 *
 */
void GNSDatagram::
set_connection(const PT(GNSConnection) &connection) {
  _connection = connection;
}

/**
 *
 */
PT(GNSConnection) GNSDatagram::
get_connection() const {
  return _connection;
}

/**
 *
 */
void GNSDatagram::
set_address(const GNSNetAddress &address) {
  _address = address;
}

/**
 *
 */
const GNSNetAddress &GNSDatagram::
get_address() const {
  return _address;
}

// ---------------------------------------------------------------
// GNSConnectionReader::ReaderThread
// ---------------------------------------------------------------

/**
 *
 */
GNSConnectionReader::ReaderThread::
ReaderThread(GNSConnectionReader *reader, int thread_index) :
  Thread(std::string("GNSReader") + std::to_string(thread_index),
         std::string("GNSReader") + std::to_string(thread_index)),
  _reader(reader),
  _thread_index(thread_index)
{
}

/**
 *
 */
void GNSConnectionReader::ReaderThread::
thread_main() {
  _reader->thread_run();
}

// ---------------------------------------------------------------
// GNSConnectionReader
// ---------------------------------------------------------------

/**
 * Constructs a GNSConnectionReader.  If num_threads is 0, reading is manual
 * via poll().  If num_threads > 0, background threads will poll continuously.
 */
GNSConnectionReader::
GNSConnectionReader(GNSConnectionManager *manager, int num_threads) :
  _manager(manager),
  _shutdown(false),
  _polling(num_threads == 0),
  _max_queue_size(100000),
  _queue_mutex("GNSConnectionReader::_queue_mutex")
{
  nassertv(manager != nullptr);
  manager->add_reader(this);

  for (int i = 0; i < num_threads; i++) {
    PT(ReaderThread) thread = new ReaderThread(this, i);
    _threads.push_back(thread);
    thread->start(TP_normal, true);
  }
}

/**
 *
 */
GNSConnectionReader::
~GNSConnectionReader() {
  shutdown();
  if (_manager != nullptr) {
    _manager->remove_reader(this);
    _manager = nullptr;
  }
}

/**
 * Returns true if at least one datagram is available for retrieval.
 */
bool GNSConnectionReader::
data_available() const {
  LightMutexHolder holder(_queue_mutex);
  return !_queue.empty();
}

/**
 * Retrieves the next datagram from the queue.  Returns true on success.
 */
bool GNSConnectionReader::
get_data(GNSDatagram &result) {
  LightMutexHolder holder(_queue_mutex);
  if (_queue.empty()) {
    return false;
  }
  result = _queue.front();
  _queue.pop_front();
  return true;
}

/**
 * Manually polls the GNS poll group for incoming messages.  Only useful when
 * num_threads is 0.  Call this once per frame or more often.
 */
void GNSConnectionReader::
poll() {
  do_poll();
}

/**
 * Returns true if this reader is manual-poll mode (no threads).
 */
bool GNSConnectionReader::
is_polling() const {
  return _polling;
}

/**
 * Returns the number of reader threads.
 */
int GNSConnectionReader::
get_num_threads() const {
  return (int)_threads.size();
}

/**
 *
 */
GNSConnectionManager *GNSConnectionReader::
get_manager() const {
  return _manager;
}

/**
 * Sets the maximum number of datagrams that may be queued.
 */
void GNSConnectionReader::
set_max_queue_size(int max_size) {
  _max_queue_size = max_size;
}

/**
 * Returns the maximum queue size.
 */
int GNSConnectionReader::
get_max_queue_size() const {
  return _max_queue_size;
}

/**
 * Shuts down all reader threads.
 */
void GNSConnectionReader::
shutdown() {
  _shutdown = true;
  for (auto &thread : _threads) {
    thread->join();
  }
  _threads.clear();
}

/**
 * The thread function for background reader threads.
 */
void GNSConnectionReader::
thread_run() {
  while (!_shutdown) {
    do_poll();
    Thread::sleep(gns_poll_timeout_ms * 0.001);
  }
}

/**
 * Performs the actual poll of the GNS poll group, converting incoming
 * SteamNetworkingMessage_t messages into GNSDatagrams.
 */
void GNSConnectionReader::
do_poll() {
  if (_manager == nullptr || _manager->get_interface() == nullptr) {
    return;
  }

  ISteamNetworkingSockets *iface = _manager->get_interface();
  HSteamNetPollGroup poll_group = _manager->get_poll_group();
  if (poll_group == k_HSteamNetPollGroup_Invalid) {
    return;
  }

  // Receive up to 64 messages at once.
  SteamNetworkingMessage_t *messages[64];
  int num_messages = iface->ReceiveMessagesOnPollGroup(
    poll_group, messages, 64);

  for (int i = 0; i < num_messages; i++) {
    SteamNetworkingMessage_t *msg = messages[i];

    GNSDatagram datagram(msg->m_pData, msg->m_cbSize);

    // Try to find the GNSConnection for this message.
    PT(GNSConnection) conn = _manager->find_connection(msg->m_conn);
    if (conn != nullptr) {
      datagram.set_connection(conn);
      datagram.set_address(conn->get_address());
    }

    {
      LightMutexHolder holder(_queue_mutex);
      if ((int)_queue.size() < _max_queue_size) {
        _queue.push_back(datagram);
      } else {
        gns_cat.warning()
          << "GNS reader queue full, dropping datagram\n";
      }
    }

    msg->Release();
  }
}
