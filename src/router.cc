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
  // The “match”:The Router searches the routing table to find the routes that match the datagram’s
  // destination address. By “match,” we mean the most-significant prefix length bits of
  // the destination address are identical to the most-significant prefix length bits of the
  // route prefix.

  // The “action”: what to do if the route matches and is chosen. If the router is
  // directly attached to the network in question, the next hop will be an empty optional.
  // In that case, the next hop is the datagram’s destination address. But if the router is
  // connected to the network in question through some other router, the next hop will
  // contain the IP address of the next router along the path. The interface num gives the
  // index of the router’s NetworkInterface that should use to send the datagram to the
  // next hop. You can access this interface with the interface(interface num) method.
  
  // Your code here.
  size_t interface_size = _interfaces.size();

  for (size_t in_num = 0; in_num < interface_size; in_num++) {

    std::queue<InternetDatagram> datagrams = interface(in_num).get()->datagrams_received();

    while (!datagrams.empty()) {
      auto datagram = datagrams.front();
      // Find the route that matches the datagram's destination address
      uint32_t dst =  datagram.header.dst;
      uint8_t ttl = (datagram.header.ttl--);
      if (ttl <= 0) {
        datagrams.pop();
        continue;
      }
      for (auto route : _route_table) {
        const uint8_t prefix_length = route.second.prefix_length;
        const uint32_t route_prefix = route.first;
        if ((dst & ~((1 << (32 - prefix_length)) - 1)) == (route_prefix)) {
          // The datagram's destination address matches the route prefix
          // Send the datagram out the corresponding interface
          size_t out_num = route.second.interface_num;
          if (route.second.next_hop.has_value()) {
            // Send the new datagram out the corresponding interface
            interface(out_num)->send_datagram(datagram,*route.second.next_hop);
          } else if(!route.second.next_hop.has_value()) {
            Address dst_address = Address::from_ipv4_numeric(datagram.header.dst);
            interface(out_num)->send_datagram(datagram,dst_address);
          }
          break;
        }
      }

      datagrams.pop();
    }

  }
}
