// router.hh
#pragma once

#include <memory>
#include <optional>

#include "exception.hh"
#include "network_interface.hh"

struct RouteTableEntry
{
  // uint32_t route_prefix;
  uint8_t prefix_length;
  std::optional<Address> next_hop;
  size_t interface_num;

  // 默认构造函数
  RouteTableEntry() : prefix_length(0), next_hop(std::nullopt), interface_num(0) {}

  RouteTableEntry(uint8_t pl, std::optional<Address> nh, size_t in)
      : prefix_length(pl), next_hop(nh), interface_num(in) {}

  bool operator<( const RouteTableEntry& other ) const
  {
    return prefix_length > other.prefix_length;
  }
};

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};

  std::map<uint32_t, RouteTableEntry> _route_table {};
};