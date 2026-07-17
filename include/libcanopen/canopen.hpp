#ifndef LIBCANOPEN_CANOPEN_HPP_
#define LIBCANOPEN_CANOPEN_HPP_

#include <stdint.h>

#include "libcanopen/common.hpp"

namespace libcanopen {

class Node {
public:
    explicit Node(const TransportApi& transport);

    [[nodiscard]] int16_t init(uint32_t bitrate);
    [[nodiscard]] int16_t sendNmtStart(uint8_t node_id);
    [[nodiscard]] int16_t sendNmtStop(uint8_t node_id);
    [[nodiscard]] int16_t sendNmtPreOperational(uint8_t node_id);
    [[nodiscard]] int16_t sendNmtReset(uint8_t node_id);
    [[nodiscard]] int16_t spinOnce(uint16_t expected_tpdo_id, uint8_t expected_dlc,
                                   uint8_t max_frames);
    [[nodiscard]] const Stats& stats() const;
    [[nodiscard]] bool getLastTpdo(Frame& frame) const;

private:
    [[nodiscard]] int16_t sendNmt(uint8_t command, uint8_t node_id);

    TransportApi _transport;
    Stats _stats{};
    Frame _last_tpdo{};
    bool _initialized{false};
    bool _has_last_tpdo{false};
};

}  // namespace libcanopen

#endif  // LIBCANOPEN_CANOPEN_HPP_
