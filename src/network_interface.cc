// network_interface.cc
#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , datagrams_received_() // 这里按顺序初始化
  , arp_table_()          // 这里按顺序初始化
  , pending_datagrams_()  // 这里按顺序初始化
  , current_time_( 0 )    // 确保在最后初始化
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_hop_ip = next_hop.ipv4_numeric();

  auto it = arp_table_.find( next_hop_ip );
  if ( it != arp_table_.end() && std::get<1>( it->second ) > current_time_ ) {
    // 有有效的ARP映射，可以发送数据报
    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.header.src = ethernet_address_;
    frame.header.dst = std::get<0>( it->second );

    Serializer serializer;
    dgram.serialize( serializer );
    frame.payload = serializer.output();

    transmit( frame );
  } else {
    // 没有有效的ARP映射，发送ARP请求
    if ( it == arp_table_.end() || current_time_ - std::get<1>( it->second ) >= 5000 ) {
      ARPMessage arp_request;
      arp_request.opcode = ARPMessage::OPCODE_REQUEST;
      arp_request.sender_ethernet_address = ethernet_address_;
      arp_request.sender_ip_address = ip_address_.ipv4_numeric();
      arp_request.target_ethernet_address = {};
      arp_request.target_ip_address = next_hop_ip;

      EthernetFrame arp_frame;
      arp_frame.header.type = EthernetHeader::TYPE_ARP;
      arp_frame.header.src = ethernet_address_;
      arp_frame.header.dst = ETHERNET_BROADCAST;

      Serializer arp_serializer;
      arp_request.serialize( arp_serializer );
      arp_frame.payload = arp_serializer.output();

      transmit( arp_frame );
      arp_table_[next_hop_ip] = { EthernetAddress(), current_time_, false };
    }

    // 将数据报保存到待处理队列
    pending_datagrams_.emplace( dgram, next_hop );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    Parser parser( frame.payload );
    try {
      dgram.parse( parser );
      datagrams_received_.push( dgram );
    } catch ( const std::exception& e ) {
      // 解析失败，忽略这个数据报
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    Parser parser( frame.payload );
    try {
      arp_msg.parse( parser );
      arp_table_[arp_msg.sender_ip_address] = { arp_msg.sender_ethernet_address, current_time_ + 30000, true };

      if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST
           && arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage arp_reply;
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply.sender_ethernet_address = ethernet_address_;
        arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
        arp_reply.target_ip_address = arp_msg.sender_ip_address;

        EthernetFrame reply_frame;
        reply_frame.header.type = EthernetHeader::TYPE_ARP;
        reply_frame.header.src = ethernet_address_;
        reply_frame.header.dst = arp_msg.sender_ethernet_address;

        Serializer serializer;
        arp_reply.serialize( serializer );
        reply_frame.payload = serializer.output();

        transmit( reply_frame );
      }

      size_t queue_size = pending_datagrams_.size();
      for ( size_t i = 0; i < queue_size; ++i ) {
        auto [dgram, next_hop] = pending_datagrams_.front();
        pending_datagrams_.pop();

        if ( next_hop.ipv4_numeric() == arp_msg.sender_ip_address ) {
          EthernetFrame datagram_frame; // 重命名局部变量
          datagram_frame.header.type = EthernetHeader::TYPE_IPv4;
          datagram_frame.header.src = ethernet_address_;
          datagram_frame.header.dst = arp_msg.sender_ethernet_address;

          Serializer serializer;
          dgram.serialize( serializer );
          datagram_frame.payload = serializer.output();

          transmit( datagram_frame );
        } else {
          pending_datagrams_.push( { dgram, next_hop } );
        }
      }
    } catch ( const std::exception& e ) {
      // 解析失败，忽略这个ARP消息
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 处理越界问题
  if ( current_time_ + ms_since_last_tick < current_time_ ) {
    // 如果发生越界，重置 current_time_
    current_time_ = 0; // 或者设置为某个合理的起始值
  } else {
    current_time_ += ms_since_last_tick;
  }

  // 清除过期的 ARP 表项
  for ( auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    if ( std::get<2>( it->second ) == true && std::get<1>( it->second ) <= current_time_ ) {
      it = arp_table_.erase( it );
    } else {
      ++it;
    }
  }
}
