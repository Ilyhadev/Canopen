#include "libcanopen/fdcan.hpp"

#include <string.h>

namespace {

constexpr size_t MAX_INTERFACES = 2U;

struct Interface {
    FDCAN_HandleTypeDef* handle;
    uint8_t interface_id;
    uint32_t errors;
};

struct Context {
    Interface interfaces[MAX_INTERFACES];
    size_t count;
    size_t next_receive;
    bool started;
};

Context context{};

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
    if (ctx == nullptr || ctx->count == 0U || ctx->started) {
        return -1;
    }
    for (size_t idx = 0U; idx < ctx->count; idx++) {
        if (HAL_FDCAN_ConfigGlobalFilter(ctx->interfaces[idx].handle,
                                         FDCAN_ACCEPT_IN_RX_FIFO0,
                                         FDCAN_REJECT,
                                         FDCAN_REJECT_REMOTE,
                                         FDCAN_REJECT_REMOTE) != HAL_OK ||
            HAL_FDCAN_Start(ctx->interfaces[idx].handle) != HAL_OK) {
            ctx->interfaces[idx].errors++;
            return -1;
        }
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
    bool sent = false;
    for (size_t idx = 0U; idx < ctx->count; idx++) {
        FDCAN_TxHeaderTypeDef header{};
        header.Identifier = frame.id;
        header.IdType = FDCAN_STANDARD_ID;
        header.TxFrameType = FDCAN_DATA_FRAME;
        header.DataLength = frame.data_len;
        header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        header.BitRateSwitch = FDCAN_BRS_OFF;
        header.FDFormat = FDCAN_CLASSIC_CAN;
        header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        if (sendFrame(ctx->interfaces[idx].handle, header, frame.data) == HAL_OK) {
            sent = true;
        } else {
            ctx->interfaces[idx].errors++;
        }
    }
    return sent ? 1 : -1;
}

int16_t receive(void* raw_context, libcanopen::Frame& frame) {
    auto* ctx = static_cast<Context*>(raw_context);
    if (ctx == nullptr || !ctx->started) {
        return -1;
    }
    for (size_t attempt = 0U; attempt < ctx->count; attempt++) {
        const size_t idx = (ctx->next_receive + attempt) % ctx->count;
        FDCAN_RxHeaderTypeDef header{};
        uint8_t data[8]{};
        if (HAL_FDCAN_GetRxMessage(ctx->interfaces[idx].handle,
                                   FDCAN_RX_FIFO0,
                                   &header,
                                   data) != HAL_OK) {
            continue;
        }
        // STM32 HAL versions disagree on whether the received DLC is encoded
        // in bits 16..19 or returned as a decoded value.
        const uint8_t data_len = header.DataLength <= 0xFU
                                     ? static_cast<uint8_t>(header.DataLength)
                                     : static_cast<uint8_t>(header.DataLength >> 16U);
        if (header.IdType != FDCAN_STANDARD_ID || header.RxFrameType != FDCAN_DATA_FRAME ||
            header.FDFormat != FDCAN_CLASSIC_CAN || header.Identifier > 0x7FFU ||
            data_len > sizeof(frame.data)) {
            ctx->interfaces[idx].errors++;
            continue;
        }
        frame.id = static_cast<uint16_t>(header.Identifier);
        frame.data_len = data_len;
        frame.type = libcanopen::FrameType::DATA;
        frame.timestamp_us = static_cast<uint64_t>(HAL_GetTick()) * 1000ULL;
        memcpy(frame.data, data, data_len);
        ctx->next_receive = (idx + 1U) % ctx->count;
        return 1;
    }
    return 0;
}

uint64_t getTimeUs(void* raw_context) {
    (void)raw_context;
    return static_cast<uint64_t>(HAL_GetTick()) * 1000ULL;
}

}  // namespace

int16_t canopenFdcanConfigure(const CanopenFdcanInterfaceConfig* interfaces,
                              const size_t interface_count) {
    if (context.started || interfaces == nullptr || interface_count == 0U ||
        interface_count > MAX_INTERFACES) {
        return -1;
    }
    for (size_t idx = 0U; idx < interface_count; idx++) {
        if (interfaces[idx].handle == nullptr) {
            return -1;
        }
        for (size_t other = 0U; other < idx; other++) {
            if (interfaces[idx].handle == interfaces[other].handle ||
                interfaces[idx].interface_id == interfaces[other].interface_id) {
                return -1;
            }
        }
    }
    context = {};
    context.count = interface_count;
    for (size_t idx = 0U; idx < interface_count; idx++) {
        context.interfaces[idx].handle = interfaces[idx].handle;
        context.interfaces[idx].interface_id = interfaces[idx].interface_id;
    }
    return 0;
}

libcanopen::TransportApi canopenFdcanGetTransportApi() {
    return {
        .context = &context,
        .init = init,
        .send = send,
        .receive = receive,
        .get_time_us = getTimeUs,
    };
}
