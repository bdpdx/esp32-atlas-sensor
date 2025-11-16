//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

// suppress redeclaration of err_t error
#define LWIP_ERR_T      int

#include <driver/dac_oneshot.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <esp_crt_bundle.h>
#include <esp_debug_helpers.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_https_ota.h>
#include <esp_intr_alloc.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_websocket_client.h>
#include <hal/dac_types.h>
#include <soc/gpio_num.h>
#include <soc/soc_caps.h>
