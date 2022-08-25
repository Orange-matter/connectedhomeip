#include "Button.h"

#if CONFIG_IDF_TARGET_ESP32S3
#define GPIO_INPUT_IO_0 0
#elif CONFIG_IDF_TARGET_ESP32C3
   #define GPIO_INPUT_IO_0 9
#elif 
#warning GPIOs not defined
#endif

#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO_0)
#define ESP_INTR_FLAG_DEFAULT 0

static const char * TAG = "Button";

Button AppButton;
TimerHandle_t mbuttonTimer;

extern bool isRssiFakeMode;

static Button::ButtonActionCallback button_rising_handler = nullptr;
static Button::ButtonActionCallback button_falling_handler = nullptr;
static Button::ButtonTimerCallback timer_handler = nullptr;

static void IRAM_ATTR gpio_isr_handler(void * arg)
{
    int state = gpio_get_level(static_cast<gpio_num_t>(GPIO_INPUT_IO_0));

    if ((state == 0) && (button_falling_handler != nullptr))
    {
        BaseType_t taskWoken = pdFALSE;
        xTimerStartFromISR(mbuttonTimer, &taskWoken); // If the timer had already been started ,restart it will reset its expiry time

        button_falling_handler();
    }
    else if((state == 1) && (button_rising_handler != nullptr)) {
        BaseType_t taskWoken = pdFALSE;
        xTimerStopFromISR(mbuttonTimer, &taskWoken);
       
        button_rising_handler();
    }
}

void Button::Init()
{
    /* Initialize button interrupt*/
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // interrupt of rising edge
    //io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    // bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // enable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(static_cast<gpio_num_t>(GPIO_INPUT_IO_0), gpio_isr_handler, (void *) GPIO_INPUT_IO_0);


    isRssiFakeMode = false;
    ESP_LOGI(TAG, "Button initialized..");

}

void Button::SetButtonFallingCallback(ButtonActionCallback button_callback)
{
    if (button_callback != nullptr)
    {
        button_falling_handler = button_callback;
    }
}

void Button::SetButtonRisingCallback(ButtonActionCallback button_callback)
{
    if (button_callback != nullptr)
    {
        button_rising_handler = button_callback;
    }
}

void Button::SetTimerCallback(ButtonTimerCallback timer_callback)
{
    if (timer_callback != nullptr)
    {
        timer_handler = timer_callback;
        int gpioNum = 0;
        mbuttonTimer = xTimerCreate("BtnTmr",               // Just a text name, not used by the RTOS kernel
                                pdMS_TO_TICKS(2000),      // timer period
                                false,                  // no timer reload (==one-shot)
                                (void *) (int) gpioNum, // init timer id = gpioNum index
                                timer_handler           // timer callback handler (all buttons use
                                                        // the same timer cn function)
        );

    }
}

void Button::SetRssiState(bool state)
{
    rssiState = state;
}

bool Button::RssiState()
{
    return rssiState;
}

void Button::SetRssiFakeMode(bool fakeMode)
{
    ESP_LOGW(TAG, "RSSI set:%d", fakeMode);
    isRssiFakeMode = fakeMode;
}

bool Button::getRssiFakeMode()
{
    ESP_LOGW(TAG, "RSSI get:%d", isRssiFakeMode);
    return isRssiFakeMode;
}