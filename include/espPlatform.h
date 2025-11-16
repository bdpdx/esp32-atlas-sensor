//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#if PLATFORM_PICO

#elif PLATFORM_WROVER

#elif PLATFORM_WROVER_KIT
    // built-in rgb led
    #define LED_BLUE_GPIO                   GPIO_NUM_4
    #define LED_GREEN_GPIO                  GPIO_NUM_2
    #define LED_RED_GPIO                    GPIO_NUM_0
#else
    #error "Please define PLATFORM_* in CMakeLists.txt"
#endif
