#ifndef LIBCANOPEN_LSS_HPP_
#define LIBCANOPEN_LSS_HPP_

#include <stdint.h>

#include "libcanopen/common.hpp"

namespace libcanopen {

struct LssAddress {
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    uint32_t serial_number;
};

enum class LssState : uint8_t {
    WAITING = 0U,
    CONFIGURATION = 1U,
};

enum class LssBitrate : uint8_t {
    KBIT_1000 = 0U,
    KBIT_800 = 1U,
    KBIT_500 = 2U,
    KBIT_250 = 3U,
    KBIT_125 = 4U,
    KBIT_100 = 5U,
    KBIT_50 = 6U,
    KBIT_20 = 7U,
    KBIT_10 = 8U,
};

enum class LssStatus : uint8_t {
    SUCCESS = 0U,
    INVALID_ARGUMENT,
    TIMEOUT,
    TRANSPORT_ERROR,
    UNSUPPORTED,
    SERVER_REJECTED,
    INVALID_RESPONSE,
};

struct LssResult {
    LssStatus status;
    uint8_t error_code;
    uint8_t error_extension;
};

class LssMaster {
public:
    static constexpr uint32_t DEFAULT_TIMEOUT_MS = 500U;

    explicit LssMaster(const TransportApi& transport);

    [[nodiscard]] LssResult switchStateGlobal(LssState state);
    [[nodiscard]] LssResult switchStateSelective(
        const LssAddress& address, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS);
    [[nodiscard]] LssResult configureNodeId(
        uint8_t new_node_id, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS);
    [[nodiscard]] LssResult storeConfiguration(
        uint32_t timeout_ms = DEFAULT_TIMEOUT_MS);
    [[nodiscard]] LssResult configureBitTiming(
        LssBitrate bitrate, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS);
    [[nodiscard]] LssResult activateBitTiming(uint16_t delay_ms);

private:
    [[nodiscard]] LssResult send(uint8_t command, const uint8_t* payload,
                                 uint8_t payload_len);
    [[nodiscard]] LssResult waitForResponse(uint8_t command, uint32_t timeout_ms,
                                            bool has_error_fields);

    TransportApi _transport;
};

}  // namespace libcanopen

#endif  // LIBCANOPEN_LSS_HPP_
