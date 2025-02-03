#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 超过容量上限的直接丢掉
    if (_buffer_index + _capacity <= index) {
        return;
    }
    // 已经被处理过的数据只检查是否eof
    if (index + data.length() <= _buffer_index) {
        if (eof) {
            _eof_flags = true;
        }
        if (_eof_flags && empty()) {
            _output.end_input();
        }
        return;
    }

    outOrderNode node;
    node._index = max(_buffer_index, index);
    node._len = index + data.length() - node._index;
    node._data = data.substr(node._index - index, node._len);
    // 在set中寻找什么节点能和node合并,set中按照_index从小到大存储
    auto it = _outOrderData_set.begin();
    while (it != _outOrderData_set.end()) {
        // for (auto &it : _outOrderData_set) {
        // 后面的_index只会更大,不需要再遍历
        if (node._index + node._len < it->_index) {
            break;
        }
        // 不相邻
        if (it->_index + it->_len < node._index) {
            it++;
            continue;
        }

        // 需要合并node和it
        outOrderNode node_smaller, node_bigger;
        // 使node_smaller成为node和it中_index更小的节点,bigger成为更大的节点
        if (node._index <= it->_index) {
            node_smaller = node;
            node_bigger = *it;
        } else {
            node_smaller = *it;
            node_bigger = node;
        }
        // 合并节点
        node._index = node_smaller._index;
        size_t node_bigger_data_start =
            min(node_bigger._len, node_smaller._index + node_smaller._len - node_bigger._index);
        size_t node_bigger_data_len = node_bigger._len - node_bigger_data_start;
        node._data = node_smaller._data + node_bigger._data.substr(node_bigger_data_start, node_bigger_data_len);
        node._len = node._data.length();
        //在set中移除it
        _unassembled_bytes -= it->_len;
        auto it_copy = it;
        it++;
        _outOrderData_set.erase(it_copy);
    }
    // 如果node的数据就是下一个要排列的数据,写入_output;否则插入set
    if (node._index == _buffer_index) {
        size_t write_bytes = _output.write(node._data);
        _buffer_index += write_bytes;
        // 如果node中数据长度超过output的容量,将剩下的字节插入到set中
        if (write_bytes < node._len) {
            node._index = node._index + write_bytes;
            node._data = node._data.substr(write_bytes, node._len - write_bytes);
            node._len = node._data.length();
            _outOrderData_set.insert(node);
            _unassembled_bytes += node._len;
        }
    } else {
        _outOrderData_set.insert(node);
        _unassembled_bytes += node._len;
    }
    if (eof) {
        _eof_flags = true;
    }
    if (_eof_flags && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
