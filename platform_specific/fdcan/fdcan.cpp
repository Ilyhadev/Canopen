#include "libcanopen/fdcan.hpp"

#include <string.h>

#include "main.h"

#ifndef DRONECAN_FDCAN_PRIMARY
    #define DRONECAN_FDCAN_PRIMARY 1
#endif

#ifndef CANOPEN_FDCAN_PRIMARY
    #define CANOPEN_FDCAN_PRIMARY 2
#endif

static_assert(DRONECAN_FDCAN_PRIMARY == 1 || DRONECAN_FDCAN_PRIMARY == 2,
              "DRONECAN_FDCAN_PRIMARY must be 1 or 2");
static_assert(CANOPEN_FDCAN_PRIMARY == 1 || CANOPEN_FDCAN_PRIMARY == 2,
              "CANOPEN_FDCAN_PRIMARY must be 1 or 2");
static_assert(DRONECAN_FDCAN_PRIMARY != CANOPEN_FDCAN_PRIMARY,
              "DroneCAN and CANopen cannot own the same FDCAN controller");

extern "C" FDCAN_HandleTypeDef hfdcan1;
extern "C" FDCAN_HandleTypeDef hfdcan2;

namespace {

struct Context {
    FDCAN_HandleTypeDef* handle;
    uint32_t errors;
    bool started;
};

#if CANOPEN_FDCAN_PRIMARY == 1
Context context{.handle = &hfdcan1};
#else
Context context{.handle = &hfdcan2};
#endif

HAL_StatusTypeDef sendFrame(FDCAN_HandleTypeDef* handle,
                            const FDCAN_TxHeaderTypeDef& header,
                            const uint8_t* data) {
    if (handle->Init.TxFifoQueueElmtsNbr > 0U) {
        return HAL_FDCAN_AddMessageToTxFifoQ(
            handle, &header, const_cast<uint8_t*>(data));
    }

    constexpr uint32_t MAX_TX_BUFFERS = 32U;
    const uint32_t buffer_count =
        handle->Init.TxBuffersNbr < MAX_TX_BUFFERS ? handle->Init.TxBuffersNbr : MAX_TX_BUFFERS;
    for (uint32_t idx = 0U; idx < buffer_count; idx++) {
        const uint32_t buffer = 1UL << idx;
        if (HAL_FDCAN_IsTxBufferMessagePending(handle, buffer) == 0U &&
            HAL_FDCAN_AddMessageToTxBuffer(handle, &header, data, buffer) == HAL_OK) {
            return HAL_FDCAN_EnableTxBufferRequest(handle, buffer);
        }
    }
    return HAL_ERROR;
}

int16_t init(void* raw_context, uint32_t bitrate) {
    (void)bitrate;
    auto* ctx = static_cast<Context*>(raw_context);
    if (ctx == nullptr || ctx->handle == nullptr || ctx->started) {
        return -1;
    }
    if (HAL_FDCAN_ConfigGlobalFilter(ctx->handle,
                                     FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_REJECT,
                                     FDCAN_REJECT_REMOTE,
                                     FDCAN_REJECT_REMOTE) != HAL_OK ||
        HAL_FDCAN_Start(ctx->handle) != HAL_OK) {
        ctx->errors++;
        return -1;
    }
    ctx->started = true;
    return 0;
}

int16_t send(void* raw_context, const libcanopen::Frame& frame) {
    auto* ctx = static_cast<Context*>(raw_context);
    if (ctx == nullptr || !ctx->started || frame.id > 0x7FFU || frame.data_len > 8U ||
        frame.type != libcanopen::FrameType::DATA) {
        return -1;
    }

    FDCAN_TxHeaderTypeDef header{};
    header.Identifier = frame.id;
    header.IdType = FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = frame.data_len;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_OFF;
    header.FDFormat = FDCAN_CLASSIC_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    if (sendFrame(ctx->handle, header, frame.data) != HAL_OK) {
        ctx->errors++;
        return -1;
    }
    return 1;
}

int16_t receive(void* raw_context, libcanopen::Frame& frame) {
    auto* ctx = static_cast<Context*>(raw_context);
    if (ctx == nullptr || !ctx->started) {
        return -1;
    }

    FDCAN_RxHeaderTypeDef header{};
    uint8_t data[8]{};
    if (HAL_FDCAN_GetRxMessage(ctx->handle, FDCAN_RX_FIFO0, &header, data) != HAL_OK) {
        return 0;
    }
    const uint8_t data_len = static_cast<uint8_t>(header.DataLength);
    if (header.IdType != FDCAN_STANDARD_ID || header.RxFrameType != FDCAN_DATA_FRAME ||
        header.FDFormat != FDCAN_CLASSIC_CAN || header.Identifier > 0x7FFU ||
        data_len > sizeof(frame.data)) {
        ctx->errors++;
        return 0;
    }

    frame.id = static_cast<uint16_t>(header.Identifier);
    frame.data_len = data_len;
    frame.type = libcanopen::FrameType::DATA;
    frame.timestamp_us = static_cast<uint64_t>(HAL_GetTick()) * 1000ULL;
    memcpy(frame.data, data, data_len);
    return 1;
}

uint64_t getTimeUs(void* raw_context) {
    (void)raw_context;
    return static_cast<uint64_t>(HAL_GetTick()) * 1000ULL;
}

}  // namespace

libcanopen::TransportApi canopenFdcanGetTransportApi() {
    return {
        .context = &context,
        .init = init,
        .send = send,
        .receive = receive,
        .get_time_us = getTimeUs,
    };
}
