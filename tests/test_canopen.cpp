#include "libcanopen/canopen.hpp"
#include "libcanopen/lss.hpp"

#include <assert.h>

namespace {

struct FakeTransport {
    libcanopen::Frame sent[16]{};
    libcanopen::Frame received[16]{};
    uint8_t sent_count{0U};
    uint8_t received_count{0U};
    uint8_t received_index{0U};
    uint64_t time_us{0U};
    bool send_failure{false};
    bool receive_failure{false};
};

int16_t init(void* context, const uint32_t bitrate) {
    return context != nullptr && bitrate == 250000U ? 0 : -1;
}

int16_t send(void* context, const libcanopen::Frame& frame) {
    auto* const fake = static_cast<FakeTransport*>(context);
    if (fake->send_failure || fake->sent_count >= 16U) {
        return -1;
    }
    fake->sent[fake->sent_count++] = frame;
    return 1;
}

int16_t receive(void* context, libcanopen::Frame& frame) {
    auto* const fake = static_cast<FakeTransport*>(context);
    if (fake->receive_failure) {
        return -1;
    }
    if (fake->received_index >= fake->received_count) {
        return 0;
    }
    frame = fake->received[fake->received_index++];
    return 1;
}

uint64_t getTimeUs(void* context) {
    auto* const fake = static_cast<FakeTransport*>(context);
    fake->time_us += 1000U;
    return fake->time_us;
}

libcanopen::TransportApi makeApi(FakeTransport& fake) {
    return {&fake, init, send, receive, getTimeUs};
}

libcanopen::Frame lssResponse(const uint8_t command, const uint8_t error = 0U,
                              const uint8_t extension = 0U) {
    return {
        .id = 0x7E4U,
        .data = {command, error, extension, 0U, 0U, 0U, 0U, 0U},
        .data_len = 8U,
        .type = libcanopen::FrameType::DATA,
        .timestamp_us = 0U,
    };
}

void queue(FakeTransport& fake, const libcanopen::Frame& frame) {
    assert(fake.received_count < 16U);
    fake.received[fake.received_count++] = frame;
}

void testNode() {
    FakeTransport fake{};
    libcanopen::Node node(makeApi(fake));
    assert(node.sendNmtStart(0x7FU) < 0);
    assert(node.init(250000U) == 0);
    assert(node.sendNmtStart(0U) < 0);
    assert(node.sendNmtStart(128U) < 0);

    assert(node.sendNmtStart(0x7FU) == 0);
    assert(node.sendNmtStop(0x7FU) == 0);
    assert(node.sendNmtPreOperational(0x7FU) == 0);
    assert(node.sendNmtReset(0x7FU) == 0);
    const uint8_t commands[4] = {0x01U, 0x02U, 0x80U, 0x81U};
    for (uint8_t idx = 0U; idx < 4U; idx++) {
        assert(fake.sent[idx].id == 0U && fake.sent[idx].data_len == 2U);
        assert(fake.sent[idx].data[0] == commands[idx]);
        assert(fake.sent[idx].data[1] == 0x7FU);
    }

    queue(fake, {.id = 0x1FFU,
                 .data = {},
                 .data_len = 8U,
                 .type = libcanopen::FrameType::DATA,
                 .timestamp_us = 0U});
    assert(node.spinOnce(0x1FFU, 8U, 8U) == 1);
    assert(node.stats().valid_tpdos == 1U);
    libcanopen::Frame last_tpdo{};
    assert(node.getLastTpdo(last_tpdo));
    assert(last_tpdo.id == 0x1FFU && last_tpdo.data_len == 8U);
}

void testGlobalAndSelectiveSwitch() {
    FakeTransport fake{};
    libcanopen::LssMaster lss(makeApi(fake));
    assert(lss.switchStateGlobal(libcanopen::LssState::CONFIGURATION).status ==
           libcanopen::LssStatus::SUCCESS);
    assert(fake.sent[0].id == 0x7E5U && fake.sent[0].data_len == 8U);
    assert(fake.sent[0].data[0] == 0x04U && fake.sent[0].data[1] == 1U);
    for (uint8_t idx = 2U; idx < 8U; idx++) {
        assert(fake.sent[0].data[idx] == 0U);
    }
    assert(lss.switchStateGlobal(libcanopen::LssState::WAITING).status ==
           libcanopen::LssStatus::SUCCESS);
    assert(fake.sent[1].data[1] == 0U);

    queue(fake, lssResponse(0x44U));
    const libcanopen::LssAddress address{0x11223344U, 0x55667788U, 0x90ABCDEFU,
                                         0x01234567U};
    assert(lss.switchStateSelective(address, 10U).status ==
           libcanopen::LssStatus::SUCCESS);
    const uint8_t commands[4] = {0x40U, 0x41U, 0x42U, 0x43U};
    const uint32_t values[4] = {address.vendor_id, address.product_code,
                                address.revision_number, address.serial_number};
    for (uint8_t idx = 0U; idx < 4U; idx++) {
        const libcanopen::Frame& frame = fake.sent[idx + 2U];
        assert(frame.data[0] == commands[idx] && frame.data_len == 8U);
        assert(frame.data[1] == static_cast<uint8_t>(values[idx]));
        assert(frame.data[2] == static_cast<uint8_t>(values[idx] >> 8U));
        assert(frame.data[3] == static_cast<uint8_t>(values[idx] >> 16U));
        assert(frame.data[4] == static_cast<uint8_t>(values[idx] >> 24U));
        assert(frame.data[5] == 0U && frame.data[6] == 0U && frame.data[7] == 0U);
    }

    FakeTransport timeout_fake{};
    libcanopen::LssMaster timeout_lss(makeApi(timeout_fake));
    assert(timeout_lss.switchStateSelective(address, 2U).status ==
           libcanopen::LssStatus::TIMEOUT);
}

void testConfigureNodeId() {
    FakeTransport fake{};
    libcanopen::LssMaster lss(makeApi(fake));
    assert(lss.configureNodeId(0U, 10U).status == libcanopen::LssStatus::INVALID_ARGUMENT);
    assert(lss.configureNodeId(128U, 10U).status ==
           libcanopen::LssStatus::INVALID_ARGUMENT);
    assert(fake.sent_count == 0U);

    queue(fake, {.id = 0x123U,
                 .data = {},
                 .data_len = 8U,
                 .type = libcanopen::FrameType::DATA,
                 .timestamp_us = 0U});
    queue(fake, lssResponse(0x17U));
    queue(fake, lssResponse(0x11U));
    assert(lss.configureNodeId(0x7FU, 10U).status == libcanopen::LssStatus::SUCCESS);
    assert(fake.sent[0].data[0] == 0x11U && fake.sent[0].data[1] == 0x7FU);
    assert(fake.sent[0].data_len == 8U);

    queue(fake, lssResponse(0x11U, 0xFFU, 0x42U));
    const libcanopen::LssResult rejected = lss.configureNodeId(1U, 10U);
    assert(rejected.status == libcanopen::LssStatus::SERVER_REJECTED);
    assert(rejected.error_code == 0xFFU && rejected.error_extension == 0x42U);

    libcanopen::Frame malformed = lssResponse(0x11U);
    malformed.data_len = 7U;
    queue(fake, malformed);
    assert(lss.configureNodeId(1U, 10U).status ==
           libcanopen::LssStatus::INVALID_RESPONSE);

    assert(lss.configureNodeId(1U, 2U).status == libcanopen::LssStatus::TIMEOUT);
    fake.receive_failure = true;
    assert(lss.configureNodeId(1U, 10U).status ==
           libcanopen::LssStatus::TRANSPORT_ERROR);
}

void testStoreAndUnsupportedTiming() {
    FakeTransport fake{};
    libcanopen::LssMaster lss(makeApi(fake));
    queue(fake, lssResponse(0x17U));
    assert(lss.storeConfiguration(10U).status == libcanopen::LssStatus::SUCCESS);
    assert(fake.sent[0].data[0] == 0x17U && fake.sent[0].data_len == 8U);

    queue(fake, lssResponse(0x17U, 1U));
    assert(lss.storeConfiguration(10U).error_code == 1U);
    queue(fake, lssResponse(0x17U, 2U));
    assert(lss.storeConfiguration(10U).error_code == 2U);
    queue(fake, lssResponse(0x17U, 0xFFU, 0xA5U));
    const libcanopen::LssResult manufacturer_error = lss.storeConfiguration(10U);
    assert(manufacturer_error.status == libcanopen::LssStatus::SERVER_REJECTED);
    assert(manufacturer_error.error_extension == 0xA5U);

    assert(lss.storeConfiguration(2U).status == libcanopen::LssStatus::TIMEOUT);

    const uint8_t sent_before_timing = fake.sent_count;
    assert(lss.configureBitTiming(libcanopen::LssBitrate::KBIT_250, 10U).status ==
           libcanopen::LssStatus::UNSUPPORTED);
    assert(lss.activateBitTiming(100U).status == libcanopen::LssStatus::UNSUPPORTED);
    assert(fake.sent_count == sent_before_timing);

    fake.send_failure = true;
    assert(lss.storeConfiguration(10U).status ==
           libcanopen::LssStatus::TRANSPORT_ERROR);
}

}  // namespace

int main() {
    testNode();
    testGlobalAndSelectiveSwitch();
    testConfigureNodeId();
    testStoreAndUnsupportedTiming();
    return 0;
}
