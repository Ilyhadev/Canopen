#include "libcanopen/canopen.hpp"

#include <assert.h>

namespace {

struct FakeTransport {
    libcanopen::Frame sent{};
    libcanopen::Frame received{};
    bool has_received{false};
};

int16_t init(void* context, uint32_t bitrate) {
    return context != nullptr && bitrate == 250000U ? 0 : -1;
}

int16_t send(void* context, const libcanopen::Frame& frame) {
    static_cast<FakeTransport*>(context)->sent = frame;
    return 1;
}

int16_t receive(void* context, libcanopen::Frame& frame) {
    auto* fake = static_cast<FakeTransport*>(context);
    if (!fake->has_received) {
        return 0;
    }
    frame = fake->received;
    fake->has_received = false;
    return 1;
}

uint64_t getTimeUs(void*) {
    return 42U;
}

}  // namespace

int main() {
    FakeTransport fake{};
    const libcanopen::TransportApi api{&fake, init, send, receive, getTimeUs};
    libcanopen::Node node(api);
    assert(node.init(250000U, 0U) < 0);
    assert(node.init(250000U, 0x7FU) == 0);
    assert(node.sendNmtStart() == 0);
    assert(fake.sent.id == 0U && fake.sent.data_len == 2U);
    assert(fake.sent.data[0] == 0x01U && fake.sent.data[1] == 0x7FU);

    fake.received = {.id = 0x1FFU,
                     .data = {},
                     .data_len = 8U,
                     .type = libcanopen::FrameType::DATA,
                     .timestamp_us = 0U};
    fake.has_received = true;
    assert(node.spinOnce(0x1FFU, 8U, 8U) == 1);
    assert(node.stats().valid_tpdos == 1U);
    libcanopen::Frame last_tpdo{};
    assert(node.getLastTpdo(last_tpdo));
    assert(last_tpdo.id == 0x1FFU && last_tpdo.data_len == 8U);

    fake.received.id = 0x200U;
    fake.has_received = true;
    assert(node.spinOnce(0x1FFU, 8U, 8U) == 0);
    assert(node.stats().ignored_frames == 1U);
    return 0;
}
