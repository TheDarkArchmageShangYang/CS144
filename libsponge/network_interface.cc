#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.payload() = move(dgram.serialize());
    if (_arp_map.count(next_hop_ip) && _arp_map[next_hop_ip].second > _time) {
        frame.header().dst = _arp_map[next_hop_ip].first;
        _frames_out.emplace(move(frame));
    } else {
        if (!_arps_out.count(next_hop_ip) || _arps_out[next_hop_ip] <= _time) {
            ARPMessage request_arp;
            request_arp.opcode = ARPMessage::OPCODE_REQUEST;
            request_arp.sender_ethernet_address = _ethernet_address;
            request_arp.sender_ip_address = _ip_address.ipv4_numeric();
            request_arp.target_ip_address = next_hop_ip;
            _arps_out[next_hop_ip] = _time + 5000;
            EthernetFrame request_frame;
            request_frame.header().type = EthernetHeader::TYPE_ARP;
            request_frame.header().src = _ethernet_address;
            request_frame.header().dst = ETHERNET_BROADCAST;
            request_frame.payload() = move(request_arp.serialize());
            _frames_out.emplace(move(request_frame));
        }
        _frames_waiting.emplace(next_hop_ip, move(frame));
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) == ParseResult::NoError) {
            return datagram;
        } else {
            return nullopt;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;
        if (msg.parse(frame.payload()) == ParseResult::NoError) {
            _arp_map[msg.sender_ip_address] = {msg.sender_ethernet_address, _time + 30000};
            if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage reply_arp;
                reply_arp.opcode = ARPMessage::OPCODE_REPLY;
                reply_arp.sender_ethernet_address = _ethernet_address;
                reply_arp.sender_ip_address = _ip_address.ipv4_numeric();
                reply_arp.target_ethernet_address = msg.sender_ethernet_address;
                reply_arp.target_ip_address = msg.sender_ip_address;
                EthernetFrame reply_frame;
                reply_frame.header().type = EthernetHeader::TYPE_ARP;
                reply_frame.header().src = _ethernet_address;
                reply_frame.header().dst = msg.sender_ethernet_address;
                reply_frame.payload() = move(reply_arp.serialize());
                _frames_out.emplace(move(reply_frame));
            }
            while (!_frames_waiting.empty()) {
                EthernetFrame waiting_frame = _frames_waiting.front().second;
                if (_arp_map.count(_frames_waiting.front().first) && _arp_map[_frames_waiting.front().first].second > _time) {
                    waiting_frame.header().dst = _arp_map[_frames_waiting.front().first].first;
                    _frames_waiting.pop();
                    _frames_out.emplace(move(waiting_frame));
                } else {
                    break;
                }
            }
        } else {
            return nullopt;
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time += ms_since_last_tick;
}
