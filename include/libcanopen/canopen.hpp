#ifndef LIBCANOPEN_CANOPEN_HPP_
#define LIBCANOPEN_CANOPEN_HPP_

#include <stdint.h>

namespace libcanopen {

enum class FrameType : uint8_t {
    DATA = 0,
    REMOTE,
};

struct Frame {
    uint16_t id;
    uint8_t data[8];
    uint8_t data_len;
    FrameType type;
    uint64_t timestamp_us;
};

struct TransportApi {
    void* context;
    int16_t (*init)(void* context, uint32_t bitrate);
    int16_t (*send)(void* context, const Frame& frame);
    int16_t (*receive)(void* context, Frame& frame);
    uint64_t (*get_time_us)(void* context);
};

struct Stats {
    uint32_t received_frames;
    uint32_t valid_tpdos;
    uint32_t ignored_frames;
    uint32_t transport_errors;
};

class Node {
public:
    static constexpr uint8_t MIN_NODE_ID = 1U;
    static constexpr uint8_t MAX_NODE_ID = 127U;

    explicit Node(const TransportApi& transport);

    [[nodiscard]] int16_t init(uint32_t bitrate);
    [[nodiscard]] int16_t sendNmtStart(uint8_t node_id);
    [[nodiscard]] int16_t spinOnce(uint16_t expected_tpdo_id, uint8_t expected_dlc,
                                   uint8_t max_frames);
    [[nodiscard]] const Stats& stats() const;
    [[nodiscard]] bool getLastTpdo(Frame& frame) const;

private:
    TransportApi _transport;
    Stats _stats{};
    Frame _last_tpdo{};
    bool _initialized{false};
    bool _has_last_tpdo{false};
};

}  // namespace libcanopen

#endif  // LIBCANOPEN_CANOPEN_HPP_
