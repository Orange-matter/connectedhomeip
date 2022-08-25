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

#include "driver/gpio.h"
#include "esp_log.h"

#include "driver/ledc.h"
#include "hal/ledc_types.h"

class LEDWidget
{
public:
    void Init(void);
    void Set(bool state);
    void Toggle(void);

    void SetBrightness(uint8_t brightness);
    void UpdateState();
    uint8_t GetLevel(void);
    bool IsTurnedOn(void);

private:
    bool mState;
    uint8_t mBrightness;
    gpio_num_t mGPIONum;
    void DoSet(void);
};
