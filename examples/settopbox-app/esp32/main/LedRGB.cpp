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

#include "LedRGB.h"
#include "AppTask.h"
static const char * TAG = "LedRGB";

#define BLUE_LED    GPIO_NUM_7
#define RED_LED     GPIO_NUM_8
#define GREEN_LED   GPIO_NUM_9

TimerHandle_t mledRGBTimer;
bool isBlinkingMode = false;
bool isBlinking = false;
bool isOneShoot = true;
static LedRGB::LedRgbTimerCallback ledrgb_timer_handler = nullptr;
bool mRedState, mGreenState, mBlueState;
bool mBlinkState;
led_strip_t* mStrip = nullptr;

void LedRGB::Init(void)
{
    #if defined(CONFIG_ESP32_BOARD_ONLY)
        InitLedWidget();
    #else
        InitLed();
    #endif
    mRedState   = false;
    mGreenState = false;
    mBlueState  = false;
    
    isBlinkingMode = false;
    mBlinkState = false;

    initLedTimer();
    Set();    
}

void LedRGB::InitLedWidget(void)
{
    rmt_config_t config             = RMT_DEFAULT_CONFIG_TX((gpio_num_t) CONFIG_LED_GPIO, (rmt_channel_t) CONFIG_LED_RMT_CHANNEL);
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(1, (led_strip_dev_t) config.channel);

    config.clk_div = 2;
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);

    mStrip      = led_strip_new_rmt_ws2812(&strip_config);
}

void LedRGB::InitLed(void)
{
    // Configure pin
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << BLUE_LED) | (1ULL << RED_LED) | (1ULL << GREEN_LED);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(RED_LED, 1);     
    gpio_set_level(GREEN_LED, 1); 
    gpio_set_level(BLUE_LED, 1); 
}

void LedRGB::initLedTimer()
{
    ledrgb_timer_handler = LedRGB::LedRGBTimerCallback;
    int timerId = 100;
    mledRGBTimer = xTimerCreate("LedRgbTmr",        // Just a text name, not used by the RTOS kernel
        pdMS_TO_TICKS(100),     // timer period
        false,                  // no timer reload (==one-shot)
        (void *) (int) timerId, // init timer id
        ledrgb_timer_handler    // timer callback handler 
    );
}

void LedRGB::SetLedRgbCallback(LedRgbTimerCallback ledrgb_callback)
{
    if (ledrgb_callback != nullptr)
    {
        ledrgb_timer_handler = ledrgb_callback;
    }   
}

void LedRGB::Set(bool red, bool green, bool blue, bool blinkingMode, bool oneShoot)
{
    ESP_LOGI(TAG, "Setting red to %d, green to %d, blue to %d", (red ? 1 : 0), (green ? 1 : 0), (blue ? 1 : 0));    
    isOneShoot = oneShoot;

    mRedState = red;
    mGreenState = green;
    mBlueState = blue;

    #if defined(CONFIG_ESP32_BOARD_ONLY)
        if (mStrip)
        {
            mStrip->set_pixel(mStrip, 0, mRedState ? 32 : 0, mGreenState ? 32 : 0, mBlueState ? 32: 0);
            mStrip->refresh(mStrip, 100);
        }
    #else
        gpio_set_level(RED_LED, mRedState ? 0 : 1);            
        gpio_set_level(GREEN_LED, mGreenState ? 0 : 1);
        gpio_set_level(BLUE_LED, mBlueState ? 0 : 1);

        if(blinkingMode)
        {
            BaseType_t taskWoken = pdFALSE;
            xTimerStopFromISR(mledRGBTimer, &taskWoken);
        }
    #endif

    isBlinkingMode = blinkingMode;

    if(isBlinkingMode)
    {
        if(mRedState | mGreenState | mBlueState)
        {
            //if(!isBlinking)
            {
                mBlinkState = true;
                BaseType_t taskWoken = pdFALSE;
                xTimerStartFromISR(mledRGBTimer, &taskWoken); // If the timer had already been started ,restart it will reset its expiry time
                vTimerSetReloadMode(mledRGBTimer, pdTRUE);
            }
           // isBlinking = true;
        }
      //  else isBlinking = false;
    }
}

void LedRGB::LedRGBTimerCallback(TimerHandle_t xTimer)
{
    if(isBlinkingMode)
    {
        #if defined(CONFIG_ESP32_BOARD_ONLY)

            if (mStrip)
            {
                mStrip->set_pixel(mStrip, 0, mRedState ? (mBlinkState ? 32 : 0) : 0, 
                    mGreenState ? (mBlinkState ? 32 : 0) : 0, 
                    mBlueState ? (mBlinkState ? 32 : 0) : 0);
                mStrip->refresh(mStrip, 100);
            }

        #else
            if(mRedState)
            {
                gpio_set_level(RED_LED, mBlinkState ? 0 : 1);
            }

            if(mGreenState)
            {
                gpio_set_level(GREEN_LED, mBlinkState ? 0 : 1);
            }

            if(mBlueState)
            {
                gpio_set_level(BLUE_LED, mBlinkState ? 0 : 1);
            }

            if((isOneShoot) && (!mBlinkState))            
            {
                BaseType_t taskWoken = pdFALSE;
                xTimerStopFromISR(mledRGBTimer, &taskWoken);          
            }
        #endif

        mBlinkState =! mBlinkState;
    }

    //AppEvent ledrgb_event;
    //ledrgb_event.Type     = AppEvent::kEventType_LedRGB;
    //ledrgb_event.mHandler = AppTask::AppEventHandler;
    //sAppTask.PostEvent(&ledrgb_event);
}