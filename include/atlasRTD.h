//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include "atlasSensor.h"
#include "temperatureProvider.h"

// probe calibrations:
//
// Calibration performed 7/3/20, should be done every three years
// Atlas Scientific PT-1000 RTD probe @ 0C: 1,001 ohm, at 99.5C (water boiling temp @ 500' elevation): 1,386 ohm
// Dwyer 6CTY8 @ 0C: 1,000 ohm, at 99.5C: 1,388 ohm

class AtlasRTD :
    public AtlasSensor,
    public TemperatureProvider
{

public:

    struct MemoryResponse : public AtlasSensor::Response {
        virtual err_t           parse(char *response);

        double                  value;
        int                     valueIndex;
    };

    enum class TemperatureScale { celsius, fahrenheit, kelvin };

    struct TemperatureScaleResponse : public AtlasSensor::Response {
        virtual err_t           parse(char *response);

        TemperatureScale        temperatureScale = TemperatureScale::celsius;
        const char *            temperatureScaleString = "celsius";
    };

    using MemoryResponseCallback = CommandCallback;             // response will downcast to MemoryResponse &
    using TemperatureScaleResponseCallback = CommandCallback;   // response will downcast to TemperatureScaleResponse &

    virtual double              getCurrentTemperature();
#if ENABLE_ATLAS_SIMULATOR
    virtual err_t               getSimulatedReading(char *buffer, size_t bufferSize);
#endif
    virtual err_t               init(const char *name = "RTD", uint8_t i2cSlaveAddress = defaultI2CAddress, DispatchTask *task = nullptr);
    err_t                       sendCalibration(double temperature, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendClearMemory(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendGetDataLoggerInterval(bool synchronous = true, void *context = nullptr, IntResponseCallback callback = nullptr);
    // disable the data logger prior to reading memory
    err_t                       sendGetMemoryLastStoredValue(bool synchronous = true, void *context = nullptr, MemoryResponseCallback callback = nullptr);
    err_t                       sendGetMemoryNextValue(bool synchronous = true, void *context = nullptr, MemoryResponseCallback callback = nullptr);
    err_t                       sendGetTemperatureScale(bool synchronous = true, void *context = nullptr, TemperatureScaleResponseCallback callback = nullptr);
    // pass 0 to disable the data logger
    err_t                       sendSetDataLoggerInterval(int dataLoggerInterval, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendSetTemperatureScale(TemperatureScale temperatureScale, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);

    static const uint8_t        defaultI2CAddress = 0x66;   // assigned at the factory

    // TODO: remove shared() when the hardware supports two temperature sensors
    static AtlasRTD &           shared();
#if ATLAS_RTD_ENABLE_PH_SENSOR
    static AtlasRTD &           sharedPHSensor();
#endif

private:

#if ENABLE_ATLAS_SIMULATOR
    int                         dataLoggerInterval = 0;
    char                        temperatureScale = 'c';
#endif

};
