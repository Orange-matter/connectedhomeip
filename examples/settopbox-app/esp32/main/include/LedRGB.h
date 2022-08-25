/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "led_strip.h"

class LedRGB
{
public:
    typedef void (*LedRgbTimerCallback)(TimerHandle_t xTimer);

    void Init(void);
    void initLedTimer();
    void Set(bool red = false, bool green = false, bool blue = false, bool blinkingMode = false, bool oneShoot = false);
    void SetColor(bool red, bool green, bool blue, bool blinkingMode, bool oneShoot);
    void SetLedRgbCallback(LedRgbTimerCallback ledrgb_callback);
private:
    static void LedRGBTimerCallback(TimerHandle_t xTimer);
    void InitLedWidget(void);
    void InitLed(void);

    gpio_num_t mGPIONum;
};
