#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader tcp_header = seg.header();
    if ((!_syn_flag && !tcp_header.syn) || (_syn_flag && tcp_header.syn)) {
        return;
    }

    uint64_t absolute_seq = _reassembler.stream_out().bytes_written() + 1;
    uint64_t seg_absolute_seq;
    if (tcp_header.syn) {
        _syn_flag = tcp_header.syn;
        _isn = tcp_header.seqno;
        seg_absolute_seq = 0;
    } else {
        seg_absolute_seq = unwrap(tcp_header.seqno, _isn, absolute_seq) - 1;
    }
    _reassembler.push_substring(seg.payload().copy(), seg_absolute_seq, tcp_header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_flag) {
        return nullopt;
    }
    uint64_t absolute_seq = _reassembler.stream_out().bytes_written() + 1;
    if (_reassembler.stream_out().input_ended()) {
        absolute_seq++;
    }
    return _isn + absolute_seq;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
