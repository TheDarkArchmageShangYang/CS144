#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint16_t window_size = _receiver_window_size == 0 ? 1 : _receiver_window_size;
    while (_bytes_in_flight < window_size) {
        TCPSegment seg;
        if (!_syn_flag) {
            _syn_flag = true;
            seg.header().syn = true;
        }
        if (_fin_flag)
            return;
        size_t payload_size = min(window_size - _bytes_in_flight - seg.header().syn, TCPConfig::MAX_PAYLOAD_SIZE);
        seg.payload() = Buffer{_stream.read(payload_size)};
        if (seg.length_in_sequence_space() + _bytes_in_flight < window_size && _stream.eof()) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        if (seg.length_in_sequence_space() == 0)
            return;
        seg.header().seqno = next_seqno();
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        _segments_out.push(seg);
        _segments_outstanding.push(seg);
        if (!_timer_running) {
            _timer_running = true;
            _time = 0;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t absolute_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    if (absolute_ackno > _next_seqno) {
        return;
    }
    while (!_segments_outstanding.empty()) {
        TCPSegment seg = _segments_outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= absolute_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
            _time = 0;
            _rto = _initial_retransmission_timeout;
            _consecutive_retransmissions = 0;
        } else {
            break;
        }
    }
    if (!_bytes_in_flight) {
        _timer_running = false;
    }
    _receiver_window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_running) {
        return;
    }
    _time += ms_since_last_tick;
    if (_time >= _rto) {
        _segments_out.push(_segments_outstanding.front());
        _time = 0;
        if (_receiver_window_size) {
            _consecutive_retransmissions++;
            _rto = _rto << 1;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
