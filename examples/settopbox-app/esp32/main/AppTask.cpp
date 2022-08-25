/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
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

#include "AppTask.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <DeviceInfoProviderImpl.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <platform/ESP32/DiagnosticDataProviderImpl.h>

#define APP_TASK_NAME "APP"
#define APP_EVENT_QUEUE_SIZE 10
#define APP_TASK_STACK_SIZE (3072)
#define BUTTON_PRESSED 1
#define APP_LIGHT_SWITCH 1

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

static const char * TAG = "settopbox-app-task";

extern int8_t rssiFake;

TimerHandle_t mRssiTimer;
int8_t indexRssi;
int8_t tabRssi[] = {-105 /* bad */ ,-75 /* low */, -35 /* good */};
bool isDeviceStarted = false;

LedRGB ledRGB;
namespace {
constexpr EndpointId kLightEndpointId = 1;
QueueHandle_t sAppEventQueue;
TaskHandle_t sAppTaskHandle;

chip::DeviceLayer::DeviceInfoProviderImpl gExampleDeviceInfoProvider;
chip::DeviceLayer::DiagnosticDataProviderImpl gExampleDiagnosticDataProvider;
} // namespace

static AppTask::RssiStateTimerCallback rssi_timer_handler = nullptr;

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::StartAppTask()
{
    sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent));
    if (sAppEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate app event queue");
        return APP_ERROR_EVENT_QUEUE_FAILED;
    }

    // Start App task.
    BaseType_t xReturned;
    xReturned = xTaskCreate(AppTaskMain, APP_TASK_NAME, APP_TASK_STACK_SIZE, NULL, 1, &sAppTaskHandle);
    return (xReturned == pdPASS) ? CHIP_NO_ERROR : APP_ERROR_CREATE_TASK_FAILED;
}

CHIP_ERROR AppTask::Init()
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    indexRssi = 0;

    AppButton.Init();

    AppButton.SetButtonRisingCallback(ButtonRisingCallback);
    AppButton.SetButtonFallingCallback(ButtonFallingCallback);
    AppButton.SetTimerCallback(ButtonTimerCallback);

    ledRGB.Init();
    //AppLED.Init();
    //ledRGB.SetLedRgbCallback(LedTimerCallback);
    initRssiTimerCallback(RssiTimerCallback);

    chip::DeviceLayer::SetDeviceInfoProvider(&gExampleDeviceInfoProvider);
    chip::DeviceLayer::SetDiagnosticDataProvider(&gExampleDiagnosticDataProvider);
    chip::DeviceLayer::PlatformMgr().AddEventHandler(EventHandlerPlatform);

    return err;
}

void AppTask::initRssiTimerCallback(RssiStateTimerCallback timer_callback)
{
    if (timer_callback != nullptr)
    {
        rssi_timer_handler = timer_callback;
        int gpioNum = 1;
        mRssiTimer = xTimerCreate("BtnTmr",               // Just a text name, not used by the RTOS kernel
                                pdMS_TO_TICKS(60000),      // timer period
                                false,                  // no timer reload (==one-shot)
                                (void *) (int) gpioNum, // init timer id = gpioNum index
                                rssi_timer_handler           // timer callback handler (all buttons use
                                                        // the same timer cn function)
        );
    }
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;
    CHIP_ERROR err = sAppTask.Init();
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGI(TAG, "AppTask.Init() failed due to %" CHIP_ERROR_FORMAT, err.Format());
        return;
    }

    ESP_LOGI(TAG, "App Task started");

    while (true)
    {
        BaseType_t eventReceived = xQueueReceive(sAppEventQueue, &event, pdMS_TO_TICKS(10));
        while (eventReceived == pdTRUE)
        {
            sAppTask.DispatchEvent(&event);
            eventReceived = xQueueReceive(sAppEventQueue, &event, 0); // return immediately if the queue is empty
        }
    }
}

void AppTask::PostEvent(const AppEvent * aEvent)
{
    if (sAppEventQueue != NULL)
    {
        BaseType_t status;
        if (xPortInIsrContext())
        {
            BaseType_t higherPrioTaskWoken = pdFALSE;
            status                         = xQueueSendFromISR(sAppEventQueue, aEvent, &higherPrioTaskWoken);
        }
        else
        {
            status = xQueueSend(sAppEventQueue, aEvent, 1);
        }
        if (!status)
            ESP_LOGE(TAG, "Failed to post event to app task event queue");
    }
    else
    {
        ESP_LOGE(TAG, "Event Queue is NULL should never happen");
    }
}

void AppTask::DispatchEvent(AppEvent * aEvent)
{
    if (aEvent->mHandler)
    {
        aEvent->mHandler(aEvent);
    }
    else
    {
        ESP_LOGI(TAG, "Event received with no handler. Dropping event.");
    }
}

void AppTask::PushButtonEventHandler(AppEvent * aEvent)
{
    if(!isDeviceStarted) return;
    ledRGB.Set(false, false, false, false, false);
    AppButton.SetRssiState(false);
}

void AppTask::RssiActionEventHandler(AppEvent * aEvent)
{
    ESP_LOGW(TAG, "Rssi Changed");
    if(!isDeviceStarted) return;

    if(rssiFake <= tabRssi[0]){
        indexRssi = 1;
        ledRGB.Set(true, true, false);
    }
    else if((rssiFake > tabRssi[0]) && (rssiFake <= tabRssi[1])){
        indexRssi = 2;
        ledRGB.Set(false, true, false);
    }
    else
    {
        indexRssi = 0;
        ledRGB.Set(true, false, false);
    } 

    rssiFake = tabRssi[indexRssi];

    BaseType_t taskWoken = pdFALSE;
    
    if(AppButton.getRssiFakeMode()) {
        xTimerStopFromISR(mRssiTimer, &taskWoken);
    }
    else {
        AppButton.SetRssiFakeMode();
    }
    xTimerStartFromISR(mRssiTimer, &taskWoken);
}

void AppTask::AppEventHandler(AppEvent * aEvent)
{
    switch(aEvent->Type)
    {
        case  AppEvent::kEventType_LongPressButton:
            if(!AppButton.RssiState())
            {
                ConfigurationMgr().InitiateFactoryReset();
            }
            break;

        case AppEvent::kEventType_LedRGB:
            break;
        
        case AppEvent::kEventType_StopFakeRssi:
            ESP_LOGW(TAG, "Rssi State end");
            AppButton.SetRssiFakeMode(false);
            SetRssiColor();
            break;
    }
}

void AppTask::ButtonRisingCallback()
{
    AppEvent rssi_event;
    rssi_event.Type     = AppEvent::kEventType_FakeRssi;
    rssi_event.mHandler = AppTask::RssiActionEventHandler;
    sAppTask.PostEvent(&rssi_event);
}

void AppTask::ButtonFallingCallback()
{
    AppEvent button_event;
    button_event.Type     = AppEvent::kEventType_Button;
    button_event.mHandler = AppTask::PushButtonEventHandler;
    sAppTask.PostEvent(&button_event);
}

void AppTask::ButtonTimerCallback(TimerHandle_t xTimer)
{
    AppEvent button_event;
    button_event.Type     = AppEvent::kEventType_LongPressButton;
    button_event.mHandler = AppTask::AppEventHandler;
    sAppTask.PostEvent(&button_event);
}

void AppTask::LedTimerCallback(TimerHandle_t xTimer)
{
    AppEvent ledrgb_event;
    ledrgb_event.Type     = AppEvent::kEventType_LedRGB;
    ledrgb_event.mHandler = AppTask::AppEventHandler;
    sAppTask.PostEvent(&ledrgb_event);
}

void AppTask::RssiTimerCallback(TimerHandle_t xTimer)
{
    AppEvent rssi_event;
    rssi_event.Type     = AppEvent::kEventType_StopFakeRssi;
    rssi_event.mHandler = AppTask::AppEventHandler;
    sAppTask.PostEvent(&rssi_event);
}

void AppTask::SetRssiColor()
{
    int8_t rssi;
    chip::DeviceLayer::GetDiagnosticDataProvider().GetWiFiRssi(rssi);
    ESP_LOGW(TAG, ">>>>>> RSSI=%d", rssi);
    if(rssi <= tabRssi[0]){
        ledRGB.Set(true, false, false);
    }
    else if((rssiFake > tabRssi[0]) && (rssiFake <= tabRssi[1])){
        ledRGB.Set(true, true, false);
    }
    else
    {
        ledRGB.Set(false, true, false);
    } 
}

void AppTask::EventHandlerPlatform(const ChipDeviceEvent * event, intptr_t arg)
{
    switch (event->Type)
    {
    case DeviceEventType::kCHIPoBLEAdvertisingChange:
        if(event->CHIPoBLEAdvertisingChange.Result == kActivity_Started){
            ledRGB.Set(true, false, false, true, false);            
        }
        else
        {
            ledRGB.Set();
        }            
        break;

    case DeviceEventType::kInterfaceIpAddressChanged:
    case DeviceEventType::kCommissioningComplete:
        isDeviceStarted = true;
        SetRssiColor();
    break;

    case DeviceEventType::kCHIPoBLEConnectionClosed:
        ledRGB.Set(true, true, false, true, false);
        break;

    case DeviceEventType::kCHIPoBLEConnectionEstablished:    
        ledRGB.Set(false, false, true, true, false);
        break;

    default:
        ESP_LOGW(TAG, "DeviceEventType = %d", event->Type);
        break;
    }

}
