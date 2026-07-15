#ifndef LIBCANOPEN_FDCAN_HPP_
#define LIBCANOPEN_FDCAN_HPP_

#include <stddef.h>
#include <stdint.h>

#include "libcanopen/canopen.hpp"
#include "main.h"

struct CanopenFdcanInterfaceConfig {
    FDCAN_HandleTypeDef* handle;
    uint8_t interface_id;
};

int16_t canopenFdcanConfigure(const CanopenFdcanInterfaceConfig* interfaces,
                              size_t interface_count);
libcanopen::TransportApi canopenFdcanGetTransportApi();

#endif  // LIBCANOPEN_FDCAN_HPP_
