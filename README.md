# esp32-atlas-sensor

Atlas Scientific manufactures and sells I2C-accessible sensors to
read pH, EC (Electrical Conductivity), and RTD (Temperature) sensors.

The Atlas EZO series chips support both reading of the sensors and
writing of various configuration options to their chips.

The doc/ directory contains a description of their I2C protocol.

The code in this project includes source code necessary to use Atlas
EZO sensors on ESP32 chips built with Espressif's ESP-IDF operating
system (vs the Arduino shim some chips are shipped with).

The base AtlasSensor class defines an asynchronous operations framework
common to all EZO chips. Subclasses such at AtlasPH and AtlasEC inherit
from the AtlasSensor and implement operations specific to the the
individual (pH, EC, etc.) product.

There are many other files such as a dispatch framework, utilities,
etc. I've included these files for reference but I've just pulled
all these files from a larger project I've been working on.

This repo is intended to serve as an example of code I've written and
not a standalone project. As such it hasn't been compiled nor is it
intended to produce an executable.
