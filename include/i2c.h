//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include "common.h"

class I2C {

public:

    struct Device {
        uint8_t                 address;
        i2c_master_dev_handle_t handle;
    };

    typedef Device *DeviceHandle;

    I2C(I2C const &) = delete;
   ~I2C();

    void                        operator=(I2C const &) = delete;

    err_t                       init(uint32_t clockSpeed);
    err_t                       read(DeviceHandle device, uint8_t *buffer, size_t length, uint32_t timeoutMs = 1000);
    // To save space, no attempt is made to prohibit registration of two
    // devices using the same slaveAddress. Don't do that.
    err_t                       registerDevice(uint8_t address, DeviceHandle &device);
    err_t                       unregisterDevice(DeviceHandle device);
    err_t                       write(DeviceHandle device, const char *string, uint32_t timeoutMs = 1000, bool writeTerminatingNull = false);
    err_t                       write(DeviceHandle device, uint8_t *data, size_t length, uint32_t timeoutMs = 1000);

    static I2C &                shared(i2c_port_num_t portNumber);

private:

    I2C(i2c_port_num_t portNumber) : portNumber(portNumber) { };

    i2c_master_bus_handle_t     busHandle = nullptr;
    uint32_t                    clockSpeed = 0;
    i2c_port_num_t              portNumber;

};
