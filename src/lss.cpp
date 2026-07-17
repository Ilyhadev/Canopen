#include "libcanopen/lss.hpp"

namespace libcanopen {

namespace {

constexpr uint16_t LSS_REQUEST_ID = 0x7E5U;
constexpr uint16_t LSS_RESPONSE_ID = 0x7E4U;
constexpr uint8_t SWITCH_STATE_GLOBAL = 0x04U;
constexpr uint8_t CONFIGURE_NODE_ID = 0x11U;
constexpr uint8_t STORE_CONFIGURATION = 0x17U;
constexpr uint8_t SWITCH_SELECTIVE_VENDOR_ID = 0x40U;
constexpr uint8_t SWITCH_SELECTIVE_PRODUCT_CODE = 0x41U;
constexpr uint8_t SWITCH_SELECTIVE_REVISION_NUMBER = 0x42U;
constexpr uint8_t SWITCH_SELECTIVE_SERIAL_NUMBER = 0x43U;
constexpr uint8_t SWITCH_SELECTIVE_RESPONSE = 0x44U;

constexpr LssResult result(const LssStatus status, const uint8_t error_code = 0U,
                           const uint8_t error_extension = 0U) {
    return {status, error_code, error_extension};
}

void encodeUint32Le(const uint32_t value, uint8_t* const output) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
    output[2] = static_cast<uint8_t>(value >> 16U);
    output[3] = static_cast<uint8_t>(value >> 24U);
}

}  // namespace

LssMaster::LssMaster(const TransportApi& transport) : _transport(transport) {}

LssResult LssMaster::send(const uint8_t command, const uint8_t* const payload,
                          const uint8_t payload_len) {
    if (_transport.send == nullptr || _transport.get_time_us == nullptr ||
        payload_len > 7U || (payload_len > 0U && payload == nullptr)) {
        return result(LssStatus::INVALID_ARGUMENT);
    }

    Frame frame{
        .id = LSS_REQUEST_ID,
        .data = {command, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
        .data_len = 8U,
        .type = FrameType::DATA,
        .timestamp_us = _transport.get_time_us(_transport.context),
    };
    for (uint8_t idx = 0U; idx < payload_len; idx++) {
        frame.data[idx + 1U] = payload[idx];
    }
    if (_transport.send(_transport.context, frame) < 0) {
        return result(LssStatus::TRANSPORT_ERROR);
    }
    return result(LssStatus::SUCCESS);
}

LssResult LssMaster::waitForResponse(const uint8_t command, const uint32_t timeout_ms,
                                     const bool has_error_fields) {
    if (_transport.receive == nullptr || _transport.get_time_us == nullptr ||
        timeout_ms == 0U) {
        return result(LssStatus::INVALID_ARGUMENT);
    }

    const uint64_t started_at_us = _transport.get_time_us(_transport.context);
    const uint64_t timeout_us = static_cast<uint64_t>(timeout_ms) * 1000U;
    while ((_transport.get_time_us(_transport.context) - started_at_us) < timeout_us) {
        Frame frame{};
        const int16_t receive_result = _transport.receive(_transport.context, frame);
        if (receive_result < 0) {
            return result(LssStatus::TRANSPORT_ERROR);
        }
        if (receive_result == 0) {
            continue;
        }
        if (frame.id != LSS_RESPONSE_ID || frame.data_len == 0U ||
            frame.data[0] != command) {
            continue;
        }
        if (frame.type != FrameType::DATA || frame.data_len != 8U) {
            return result(LssStatus::INVALID_RESPONSE);
        }
        if (!has_error_fields || frame.data[1] == 0U) {
            return result(LssStatus::SUCCESS);
        }
        return result(LssStatus::SERVER_REJECTED, frame.data[1], frame.data[2]);
    }
    return result(LssStatus::TIMEOUT);
}

LssResult LssMaster::switchStateGlobal(const LssState state) {
    const uint8_t state_value = static_cast<uint8_t>(state);
    if (state_value > static_cast<uint8_t>(LssState::CONFIGURATION)) {
        return result(LssStatus::INVALID_ARGUMENT);
    }
    return send(SWITCH_STATE_GLOBAL, &state_value, 1U);
}

LssResult LssMaster::switchStateSelective(const LssAddress& address,
                                          const uint32_t timeout_ms) {
    if (timeout_ms == 0U) {
        return result(LssStatus::INVALID_ARGUMENT);
    }

    const uint8_t commands[4] = {SWITCH_SELECTIVE_VENDOR_ID,
                                 SWITCH_SELECTIVE_PRODUCT_CODE,
                                 SWITCH_SELECTIVE_REVISION_NUMBER,
                                 SWITCH_SELECTIVE_SERIAL_NUMBER};
    const uint32_t values[4] = {address.vendor_id, address.product_code,
                                address.revision_number, address.serial_number};
    for (uint8_t idx = 0U; idx < 4U; idx++) {
        uint8_t payload[4]{};
        encodeUint32Le(values[idx], payload);
        const LssResult send_result = send(commands[idx], payload, sizeof(payload));
        if (send_result.status != LssStatus::SUCCESS) {
            return send_result;
        }
    }
    return waitForResponse(SWITCH_SELECTIVE_RESPONSE, timeout_ms, false);
}

LssResult LssMaster::configureNodeId(const uint8_t new_node_id,
                                     const uint32_t timeout_ms) {
    if (new_node_id < MIN_NODE_ID || new_node_id > MAX_NODE_ID || timeout_ms == 0U) {
        return result(LssStatus::INVALID_ARGUMENT);
    }
    const LssResult send_result = send(CONFIGURE_NODE_ID, &new_node_id, 1U);
    if (send_result.status != LssStatus::SUCCESS) {
        return send_result;
    }
    return waitForResponse(CONFIGURE_NODE_ID, timeout_ms, true);
}

LssResult LssMaster::storeConfiguration(const uint32_t timeout_ms) {
    if (timeout_ms == 0U) {
        return result(LssStatus::INVALID_ARGUMENT);
    }
    const LssResult send_result = send(STORE_CONFIGURATION, nullptr, 0U);
    if (send_result.status != LssStatus::SUCCESS) {
        return send_result;
    }
    return waitForResponse(STORE_CONFIGURATION, timeout_ms, true);
}

LssResult LssMaster::configureBitTiming(const LssBitrate bitrate,
                                        const uint32_t timeout_ms) {
    (void)bitrate;
    (void)timeout_ms;
    return result(LssStatus::UNSUPPORTED);
}

LssResult LssMaster::activateBitTiming(const uint16_t delay_ms) {
    (void)delay_ms;
    return result(LssStatus::UNSUPPORTED);
}

}  // namespace libcanopen
