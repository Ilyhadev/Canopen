#include "libcanopen/canopen.hpp"

namespace libcanopen {

namespace {

bool isTransportValid(const TransportApi& transport) {
    return transport.init != nullptr && transport.send != nullptr &&
           transport.receive != nullptr && transport.get_time_us != nullptr;
}

}  // namespace

Node::Node(const TransportApi& transport) : _transport(transport) {}

int16_t Node::init(const uint32_t bitrate) {
    if (!isTransportValid(_transport)) {
        return -1;
    }
    if (_transport.init(_transport.context, bitrate) < 0) {
        _stats.transport_errors++;
        return -1;
    }
    _initialized = true;
    return 0;
}

int16_t Node::sendNmtStart(const uint8_t node_id) {
    if (!_initialized || node_id < MIN_NODE_ID || node_id > MAX_NODE_ID) {
        return -1;
    }
    const Frame frame{
        .id = 0x000U,
        .data = {0x01U, node_id},
        .data_len = 2U,
        .type = FrameType::DATA,
        .timestamp_us = _transport.get_time_us(_transport.context),
    };
    if (_transport.send(_transport.context, frame) < 0) {
        _stats.transport_errors++;
        return -1;
    }
    return 0;
}

int16_t Node::spinOnce(const uint16_t expected_tpdo_id,
                       const uint8_t expected_dlc,
                       const uint8_t max_frames) {
    if (!_initialized || expected_tpdo_id > 0x7FFU || expected_dlc > 8U) {
        return -1;
    }
    int16_t valid_frames = 0;
    for (uint8_t idx = 0U; idx < max_frames; idx++) {
        Frame frame{};
        const int16_t result = _transport.receive(_transport.context, frame);
        if (result == 0) {
            break;
        }
        if (result < 0) {
            _stats.transport_errors++;
            return -1;
        }
        _stats.received_frames++;
        if (frame.id <= 0x7FFU && frame.type == FrameType::DATA &&
            frame.data_len == expected_dlc && frame.id == expected_tpdo_id) {
            _stats.valid_tpdos++;
            _last_tpdo = frame;
            _has_last_tpdo = true;
            valid_frames++;
        } else {
            _stats.ignored_frames++;
        }
    }
    return valid_frames;
}

const Stats& Node::stats() const {
    return _stats;
}

bool Node::getLastTpdo(Frame& frame) const {
    if (!_has_last_tpdo) {
        return false;
    }
    frame = _last_tpdo;
    return true;
}

}  // namespace libcanopen
