#pragma once

class TemperatureProvider {

public:

    // return current temperature in Celsius
    virtual double              getCurrentTemperature() { return 20.0; }

};
