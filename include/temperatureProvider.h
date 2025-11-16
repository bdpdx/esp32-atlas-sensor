//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

class TemperatureProvider {

public:

    // return current temperature in Celsius
    virtual double              getCurrentTemperature() { return 20.0; }

};
