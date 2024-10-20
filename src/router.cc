// router.cc
#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  _route_table[route_prefix] = RouteTableEntry(prefix_length, next_hop, interface_num);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  size_t interface_size = _interfaces.size();

  for (size_t in_num = 0; in_num < interface_size; in_num++) {

    std::queue<InternetDatagram>& datagrams = interface(in_num).get()->datagrams_received();

    while (!datagrams.empty()) {
      auto datagram = datagrams.front();
      datagrams.pop();
      // Find the route that matches the datagram's destination address
      uint32_t dst =  datagram.header.dst;
      uint8_t ttl = (datagram.header.ttl--);
      if (ttl <= 1) {
        continue;
      }
      datagram.header.compute_checksum();

      uint8_t max_prefix_length = 0;
      size_t out_num = 0;
      optional<Address> next_hop;
      bool found = false;

      for (auto route : _route_table) {
        const uint8_t prefix_length = route.second.prefix_length;
        const uint32_t route_prefix = route.first;
        if ((dst &(prefix_length? ~((1 << (32 - prefix_length)) - 1):0)) == (route_prefix)) {
          if (prefix_length >= max_prefix_length) {
            max_prefix_length = prefix_length;
            out_num = route.second.interface_num;
            next_hop = route.second.next_hop;
            found = true;
          }
        }
      }
      if (found) {
        if (next_hop.has_value()) {
          // Send the new datagram out the corresponding interface
          interface(out_num)->send_datagram(datagram,*next_hop);
        } else{
          Address dst_address = Address::from_ipv4_numeric(datagram.header.dst);
          interface(out_num)->send_datagram(datagram,dst_address);
        }
      }
      else {
        if (next_hop.has_value()) {
          // Send the new datagram out the corresponding interface
          interface(0)->send_datagram(datagram,*next_hop);
        } else{
          Address dst_address = Address::from_ipv4_numeric(datagram.header.dst);
          interface(0)->send_datagram(datagram,dst_address);
        }
      }
      
    }

  }
}
