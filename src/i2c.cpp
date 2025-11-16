//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include <string.h>

#include "common.h"
#include "i2c.h"

I2C::~I2C() {
#if !ELIDE_DESTRUCTORS_FOR_SINGLETONS
    if (busHandle) i2c_del_master_bus(busHandle);
#endif
}
    
err_t I2C::init(uint32_t clockSpeed) {
    if (busHandle != nullptr) return EALREADY;

    this->clockSpeed = clockSpeed;

    i2c_master_bus_config_t config = {};
    esp_err_t err;

    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.i2c_port = portNumber;
    
    if (portNumber == I2C_NUM_0) {
        config.scl_io_num = I2C_NUM_0_SCL_GPIO;
        config.sda_io_num = I2C_NUM_0_SDA_GPIO;
    } else {
        config.scl_io_num = I2C_NUM_1_SCL_GPIO;
        config.sda_io_num = I2C_NUM_1_SDA_GPIO;
    }

    err = i2c_new_master_bus(&config, &busHandle);

    if (err) loge("I2C::init() failed: %s", esp_err_to_name(err));

    return err;
}

err_t I2C::read(DeviceHandle device, uint8_t *buffer, size_t length, uint32_t timeoutMs) {
    if (device == nullptr || buffer == nullptr) return EINVAL;

    esp_err_t err = i2c_master_receive(device->handle, buffer, length, timeoutMs);

    if (err) loge("i2c.read 0x%x returned %s", device->address, esp_err_to_name(err));

    switch (err) {
        case ESP_ERR_TIMEOUT:   err = ETIMEDOUT;    break;
        case ESP_FAIL:          err = EIO;          break;
    }

    return err;
}

err_t I2C::registerDevice(uint8_t address, DeviceHandle &device) {
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = clockSpeed,
        .scl_wait_us = 0,
        .flags = 0,
    };

    DeviceHandle deviceHandle = new Device();

    deviceHandle->address = address;

    err_t err = i2c_master_bus_add_device(busHandle, &config, &deviceHandle->handle);

    if (!err) device = deviceHandle;
    else {
        _loge("failed to register i2c device 0x%x, err is %s", address, esp_err_to_name(err));
        delete deviceHandle;
    }

    return err;
}

I2C &I2C::shared(i2c_port_num_t portNumber) {
    static I2C *singletons[I2C_NUM_MAX] = {};

    if (singletons[portNumber] == nullptr) {
        singletons[portNumber] = new I2C(portNumber);
    }

    return *singletons[portNumber];
}

err_t I2C::unregisterDevice(DeviceHandle device) {
    if (device == nullptr) return EINVAL;

    esp_err_t err = i2c_master_bus_rm_device(device->handle);

    if (err) _loge("failed to unregister i2c device 0x%x: %s", device->address, esp_err_to_name(err));
    else delete device;

    return err;
}

err_t I2C::write(DeviceHandle device, const char *string, uint32_t timeoutMs, bool writeTerminatingNull) {
    return write(device, (uint8_t *) string, strlen(string) + (writeTerminatingNull ? 1 : 0), timeoutMs);
}

err_t I2C::write(DeviceHandle device, uint8_t *data, size_t length, uint32_t timeoutMs) {
    if (device == nullptr || data == nullptr) return EINVAL;
    if (length < 1) return 0;

    esp_err_t err;

    err = i2c_master_transmit(device->handle, data, length, timeoutMs);

    if (err) _loge("i2c write %u bytes to 0x%x failed: %s", length, device->address, esp_err_to_name(err));

    switch (err) {
        case ESP_ERR_TIMEOUT:   err = ETIMEDOUT;    break;
        case ESP_FAIL:          err = EIO;          break;
    }

    return err;
}
