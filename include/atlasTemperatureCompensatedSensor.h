#pragma once

#include "atlasSensor.h"
#include "temperatureProvider.h"

class AtlasTemperatureCompensatedSensor : public AtlasSensor {

public:

    AtlasTemperatureCompensatedSensor(TemperatureProvider *temperatureProvider = nullptr);

    virtual double              getCurrentTemperature();
    bool                        isForcedTemperatureEnabled(double &forcedTemperature) { forcedTemperature = forcedDegreesC; return isForcedTemperature; }
    virtual bool                isSetTemperatureCompensationAndTakeReadingSupported();
    virtual err_t               sendGetTemperatureCompensation(bool synchronous = true, void *context = nullptr, DoubleResponseCallback callback = nullptr);
    // if temperature is forced, the sendSetTemperatureCompensation* functions will force degreesC = forcedDegreesC
    virtual err_t               sendSetTemperatureCompensation(double degreesC, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendSetTemperatureCompensationAndTakeReading(double degreesC, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    // setting setForcedTemperature has no effect on the underlying temperatureProvider.
    // If isEnabled is true, then until setForcedTemperature(false) is called:
    //  - getCurrentTemperature() always returns forcedDegreesC.
    //  - setTemperatureCompensation*() calls always use forcedDegreesC.
    //  - The automatically reenqueued reading will always compensate with forcedDegreesC.
    //  - if shouldSendSetTemperatureCompensation is also true,
    //      sendSetTemperatureCompensation(synchronous) is issued immediately.
    err_t                       setForcedTemperature(bool isEnabled, double forcedDegreesC = defaultTemperatureC, bool shouldSendSetTemperatureCompensation = true, bool synchronous = true);

    static constexpr double     defaultTemperatureC = 25.0;
    
protected:

    virtual uint32_t            getRollingMeanNumberOfValues();
    virtual uint32_t            getSetTemperatureCompensatedResponseWaitMs() { return 300; }
    virtual uint32_t            getTemperatureCompensatedReadingResponseWaitMs() { return 900; }
    virtual err_t               sendGetReading(bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior);
    virtual err_t               sendSetTemperatureCompensation(double *degreesC, bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior);
    virtual err_t               sendSetTemperatureCompensationAndTakeReading(double *degreesC, bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior);

private:

    double                      forcedDegreesC = defaultTemperatureC;
    bool                        isForcedTemperature = false;
#if ENABLE_ATLAS_SIMULATOR
    double                      temperatureCompensationDegreesC = defaultTemperatureC;
#endif
    TemperatureProvider *       temperatureProvider;

};
