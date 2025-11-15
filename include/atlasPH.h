#pragma once

#include "atlasTemperatureCompensatedSensor.h"

class AtlasPH : public AtlasTemperatureCompensatedSensor {

public:

    enum class CalibrationPoint { high, low, mid };

    struct SlopeResponse : public AtlasSensor::Response {
        virtual err_t           parse(char *response);

        double                  acidCalibrationToIdealProbe = DBL_MIN;
        double                  baseCalibrationToIdealProbe = DBL_MIN;
        double                  millivoltsZeroPointIsOffFromTrueZero = DBL_MIN;
    };

    using SlopeResponseCallback = CommandCallback;      // response will downcast to SlopeResponse &

    AtlasPH();

    virtual uint32_t            getReadingResponseWaitMs() { return 900; }
#if ENABLE_ATLAS_SIMULATOR
    virtual err_t               getSimulatedReading(char *buffer, size_t bufferSize);
#endif
    virtual err_t               init(const char *name = "pH", uint8_t i2cSlaveAddress = defaultI2CAddress, DispatchTask *task = nullptr);
    virtual bool                isSetTemperatureCompensationAndTakeReadingSupported();
    err_t                       sendCalibration(CalibrationPoint calibrationPoint, double calibrationSolutionPH, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendGetSlope(bool synchronous = true, void *context = nullptr, SlopeResponseCallback callback = nullptr);

    static AtlasPH &            shared();

#if ENABLE_ATLAS_SIMULATOR
    double                      simulatedPH = simulatedPHMin;
    double                      simulatedPHIncrement = 0.005;

    static constexpr double     simulatedPHMax = 7.0;
    static constexpr double     simulatedPHMin = 5.0;
#endif

    static const uint8_t        defaultI2CAddress = 0x63;    // assigned at the factory

};
