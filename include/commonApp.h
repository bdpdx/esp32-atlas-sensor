#pragma once

#include <hal/dac_types.h>
#include <soc/gpio_num.h>

#include "uuid.h"

/*
    ADC1_CHANNEL_0      // GPIO36, ADC1_CHANNEL_0, SENSOR_VP, Header J3 pin 4, tied to internal 10k pull-up via EN
    ADC1_CHANNEL_1,     // GPIO37, ADC1_CHANNEL_1, Header J2 pin 17, tied to internal 10k pull-up via EN
    ADC1_CHANNEL_2,     // GPIO38, ADC1_CHANNEL_2, Header J2 pin 16, tied to internal 10k pull-up via EN
    ADC1_CHANNEL_3,     // GPIO39, ADC1_CHANNEL_3, SENSOR_VN, Header J3 pin 5, tied to internal 10k pull-up via EN
    ADC1_CHANNEL_4,     // GPIO32, ADC1_CHANNEL_4, Header J2 pin 8, connected to pump 5 3.3v level shifter
    ADC1_CHANNEL_5,     // GPIO33, ADC1_CHANNEL_5, Header J2 pin 9, connected to pump 6 3.3v level shifter
    ADC1_CHANNEL_6,     // GPIO34, ADC1_CHANNEL_6, Header J2 pin 15, connected to omega 3.3v level shifter
    ADC1_CHANNEL_7,     // GPIO35, ADC1_CHANNEL_7, Header J3 pin 14, this is our ADC in from the pressure sensor
*/

// as of 2022.07.01:
// output binary size w/o exceptions, w rtti: 1022873 (enabled in menuconfig)
// output binary size w exceptions, w rrti: 1053393 delta 30520

#if CONFIG_IDF_TARGET_ESP32
    #define AUX_GPIO                            GPIO_NUM_33
    #define I2C_NUM_0_SCL_GPIO                  GPIO_NUM_19
    #define I2C_NUM_0_SDA_GPIO                  GPIO_NUM_18
    #define OMEGA_FLOW_METER_IMPULSE_GPIO       GPIO_NUM_33     // original is on GPIO 34, aux 1 sensor is on gpio 33
    #define SHIFT_REGISTER_CLOCK_GPIO           GPIO_NUM_21
    #define SHIFT_REGISTER_DATA_GPIO            GPIO_NUM_23
    #define SHIFT_REGISTER_LATCH_GPIO           GPIO_NUM_22
    #define SHIFT_REGISTER_OUTPUT_ENABLE_GPIO   GPIO_NUM_27
#elif CONFIG_IDF_TARGET_ESP32S3
    #define AUX_GPIO                            GPIO_NUM_4
    #define BALL_VALVE_DAC_CHANNEL_GPIO         GPIO_NUM_5
    #define I2C_NUM_0_SCL_GPIO                  GPIO_NUM_6
    #define I2C_NUM_0_SDA_GPIO                  GPIO_NUM_7
    #define OMEGA_FLOW_METER_IMPULSE_GPIO       GPIO_NUM_9     // original is on GPIO 34, aux 1 sensor is on gpio 33
    #define SHIFT_REGISTER_CLOCK_GPIO           GPIO_NUM_15
    #define SHIFT_REGISTER_DATA_GPIO            GPIO_NUM_16
    #define SHIFT_REGISTER_LATCH_GPIO           GPIO_NUM_17
    #define SHIFT_REGISTER_OUTPUT_ENABLE_GPIO   GPIO_NUM_18
#else
    #error "Unsupported target for commonApp pin mapping"
#endif

#define ATLAS_RTD_ENABLE_PH_SENSOR              0
#define AWS_TOPIC_PREFIX                        "fertigation"
#define BALL_VALVE_DAC_CHANNEL                  DAC_CHAN_1
#define GALLONS_REPORT_INTERVAL                 0.25
#define I2C_NUM_0_CLOCK_FREQUENCY               100000
#define I2C_NUM_1_CLOCK_FREQUENCY               100000
#define I2C_NUM_1_SCL_GPIO                      GPIO_NUM_NC     // unused for fertigation project v1
#define I2C_NUM_1_SDA_GPIO                      GPIO_NUM_NC     // unused for fertigation project v1
#define LOG_LEVEL_LOCAL                         ESP_LOG_VERBOSE
#define NUMBER_OF_PUMPS                         8
#define NUMBER_OF_RELAYS                        40
#define NUMBER_OF_SOLENOIDS                     32
#define PRESSURE_SENSOR_ADC_CHANNEL             ADC_CHANNEL_7   // GPIO 35 on ESP32 ADC1, GPIO 84 on ESP32-S3 ADC1
#define PRESSURE_SENSOR_ADC_UNIT                ADC_UNIT_1
#define SHIFT_REGISTER_CLEAR_GPIO               GPIO_NUM_NC

extern double bootTime;
const char *chipId();
const uuid_t *deviceUuid();
const char *deviceUuidString();
