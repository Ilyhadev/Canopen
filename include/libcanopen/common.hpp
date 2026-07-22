#ifndef LIBCANOPEN_COMMON_HPP_
#define LIBCANOPEN_COMMON_HPP_

#include <stdint.h>

namespace libcanopen {

inline constexpr uint8_t MAX_NODE_ID = 127U;

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

}  // namespace libcanopen

#endif  // LIBCANOPEN_COMMON_HPP_
