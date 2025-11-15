#pragma once

#include "atlasTemperatureCompensatedSensor.h"

class AtlasEC : public AtlasTemperatureCompensatedSensor {

public:

    enum class CalibrationPoint { dry, high, low };

    struct ParametersResponse : public AtlasSensor::Response {
        virtual err_t           parse(char *response);

        bool                    isConductivityEnabled = false;
        bool                    isSalinityEnabled = false;
        bool                    isSpecificGravityEnabled = false;
        bool                    isTotalDissolvedSolidsEnabled = false;
        int                     readingResponseFieldIndexForConductivity = -1;
        int                     readingResponseFieldIndexForSalinity = -1;
        int                     readingResponseFieldIndexForSpecificGravity = -1;
        int                     readingResponseFieldIndexForTotalDissolvedSolids = -1;
    };

    using ParametersResponseCallback = CommandCallback;     // response will downcast to ParametersResponse &

    AtlasEC();

    // ec value is returned in µS/cm, but most conversion functions require mS/cm
    virtual double              convertReadingResponseToDouble(char *response);
#if ENABLE_ATLAS_SIMULATOR
    virtual err_t               getSimulatedReading(char *buffer, size_t bufferSize);
#endif
    void                        handleGetParametersResponse(ParametersResponse &response);
    virtual err_t               init(const char *name = "EC", uint8_t i2cSlaveAddress = defaultI2CAddress, DispatchTask *task = nullptr);
    virtual bool                isSetTemperatureCompensationAndTakeReadingSupported();
    err_t                       sendCalibrateDry(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    // either calibrate at 25C or adjust the calibrationSolutionEC accordingly
    // calibrationSolutionEC is in µS/s
    err_t                       sendCalibrateHigh(double calibrationSolutionEC, double solutionTemperatureC = 25.0, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendCalibrateLow(double calibrationSolutionEC, double solutionTemperatureC = 25.0, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendCalibrateSingle(double calibrationSolutionEC, double solutionTemperatureC = 25.0, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendGetParameters(bool synchronous = true, void *context = nullptr, ParametersResponseCallback callback = nullptr);
    err_t                       sendGetProbeKValue(bool synchronous = true, void *context = nullptr, DoubleResponseCallback callback = nullptr);
    err_t                       sendGetTotalDissolvedSolidsConversionFactor(bool synchronous = true, void *context = nullptr, DoubleResponseCallback callback = nullptr);
    err_t                       sendSetConductivity(bool isEnabled, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendSetProbeKValue(double K, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendSetSalinity(bool isEnabled, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendSetSpecificGravity(bool isEnabled, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendSetTotalDissolvedSolids(bool isEnabled, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    err_t                       sendSetTotalDissolvedSolidsConversionFactor(double conversionFactor, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);

    static AtlasEC &            shared();

    static const uint8_t        defaultI2CAddress = 0x64;    // assigned at the factory

protected:

    err_t                       sendCalibration(const char *prefix, double calibrationSolutionEC, double solutionTemperatureC, bool synchronous, void *context, CommandCallback callback);

private:

    bool                        isConductivityEnabled = true;
    bool                        isSalinityEnabled = false;
    bool                        isSpecificGravityEnabled = false;
    bool                        isTotalDissolvedSolidsEnabled = false;
    int                         readingResponseFieldIndexForConductivity = -1;
    int                         readingResponseFieldIndexForSalinity = -1;
    int                         readingResponseFieldIndexForSpecificGravity = -1;
    int                         readingResponseFieldIndexForTotalDissolvedSolids = -1;

#if ENABLE_ATLAS_SIMULATOR
    double                      probeKValue = 1.0;
    double                      totalDissolvedSolidsConversionFactor = 0.54;
#endif

};
