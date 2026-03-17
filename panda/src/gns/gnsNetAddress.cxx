/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gnsNetAddress.cxx
 * @author thetestgame
 * @date 2026-03-17
 */

#include "gnsNetAddress.h"

#include <cstring>

TypeHandle GNSNetAddress::_type_handle;

/**
 *
 */
GNSNetAddress::
GNSNetAddress() {
  _addr.Clear();
}

/**
 *
 */
GNSNetAddress::
GNSNetAddress(const GNSNetAddress &copy) :
  _addr(copy._addr)
{
}

/**
 *
 */
GNSNetAddress::
GNSNetAddress(const SteamNetworkingIPAddr &addr) :
  _addr(addr)
{
}

/**
 * Sets up the address from a hostname (or dotted IP string) and port.
 */
void GNSNetAddress::
set_host(const std::string &hostname, uint16_t port) {
  _addr.Clear();
  if (!_addr.ParseString(hostname.c_str())) {
    _addr.Clear();
  }
  _addr.m_port = port;
}

/**
 * Sets up the address to listen on any interface at the given port.
 */
void GNSNetAddress::
set_any(uint16_t port) {
  _addr.Clear();
  _addr.m_port = port;
}

/**
 * Returns the IP address as a string.
 */
std::string GNSNetAddress::
get_ip_string() const {
  char buf[SteamNetworkingIPAddr::k_cchMaxString];
  _addr.ToString(buf, sizeof(buf), false);
  return std::string(buf);
}

/**
 * Returns the port number.
 */
uint16_t GNSNetAddress::
get_port() const {
  return _addr.m_port;
}

/**
 * Returns true if this is a wildcard (any) address.
 */
bool GNSNetAddress::
is_any() const {
  return _addr.IsIPv6AllZeros();
}

/**
 * Resets the address to a default (cleared) state.
 */
void GNSNetAddress::
clear() {
  _addr.Clear();
}

/**
 *
 */
void GNSNetAddress::
output(std::ostream &out) const {
  char buf[SteamNetworkingIPAddr::k_cchMaxString];
  _addr.ToString(buf, sizeof(buf), true);
  out << buf;
}

/**
 *
 */
bool GNSNetAddress::
operator ==(const GNSNetAddress &other) const {
  return memcmp(&_addr, &other._addr, sizeof(_addr)) == 0;
}

/**
 *
 */
bool GNSNetAddress::
operator !=(const GNSNetAddress &other) const {
  return !operator ==(other);
}

/**
 *
 */
bool GNSNetAddress::
operator <(const GNSNetAddress &other) const {
  return memcmp(&_addr, &other._addr, sizeof(_addr)) < 0;
}

/**
 * Returns a const reference to the underlying SteamNetworkingIPAddr.
 */
const SteamNetworkingIPAddr &GNSNetAddress::
get_addr() const {
  return _addr;
}

/**
 * Sets the underlying SteamNetworkingIPAddr directly.
 */
void GNSNetAddress::
set_addr(const SteamNetworkingIPAddr &addr) {
  _addr = addr;
}
