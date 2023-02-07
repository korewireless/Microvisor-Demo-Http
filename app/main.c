/**
 *
 * Microvisor HTTP Communications Demo
 * Version 3.0.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


/*
 * STATIC PROTOTYPES
 */
static void system_clock_config(void);
static void gpio_init(void);
static void task_led(void *argument);
static void task_http(void *argument);
static void process_http_response(void);
static void log_device_info(void);
static void output_headers(uint32_t n);


/*
 * GLOBALS
 */

// This is the CMSIS/FreeRTOS thread task that flashed the USER LED
static osThreadId_t thread_led;
static const osThreadAttr_t attributes_thread_led = {
    .name = "LEDTask",
    .stack_size = 2560,
    .priority = (osPriority_t) osPriorityNormal
};

// This is the CMSIS/FreeRTOS thread task that sends HTTP requests
static osThreadId_t thread_http;
static const osThreadAttr_t attributes_thread_http = {
    .name = "HTTPTask",
    .stack_size = 5120,
    .priority = (osPriority_t) osPriorityNormal
};

/**
 *  Theses variables may be changed by interrupt handler code,
 *  so we mark them as `volatile` to ensure compiler optimization
 *  doesn't render them immutable at runtime
 */
volatile bool   received_request = false;
volatile bool   channel_was_closed = false;

/**
 * These variables are defined in `http.c`
 */
extern struct {
    MvNotificationHandle notification;
    MvNetworkHandle      network;
    MvChannelHandle      channel;
} http_handles;



/**
 *  @brief The application entry point.
 */
int main(void) {
    
    // Reset of all peripherals, Initializes the Flash interface and the sys tick.
    HAL_Init();

    // Configure the system clock
    system_clock_config();

    // Get the Device ID and build number
    log_device_info();

    // Start the network
    net_open_network();
    
    // Initialize peripherals
    gpio_init();
    
    // Init scheduler
    osKernelInitialize();

    // Create the FreeRTOS thread(s)
    thread_http = osThreadNew(task_http, NULL, &attributes_thread_http);
    thread_led  = osThreadNew(task_led,  NULL, &attributes_thread_led);

    // Start the scheduler
    osKernelStart();

    // We should never get here as control is now taken by the scheduler,
    // but just in case...
    while (1) {
        // NOP
    }
}


/**
 * @brief Get the MV clock value.
 *
 * @returns The clock value.
 */
uint32_t SECURE_SystemCoreClockUpdate(void) {
    
    uint32_t clock = 0;
    mvGetHClk(&clock);
    return clock;
}


/**
 * @brief System clock configuration.
 */
static void system_clock_config(void) {
    
    SystemCoreClockUpdate();
    HAL_InitTick(TICK_INT_PRIORITY);
}


/**
 * @brief Initialize the MCU GPIO.
 *
 * Used to flash the Nucleo's USER LED, which is on GPIO Pin PA5.
 */
static void gpio_init(void) {
    
    // Enable GPIO port clock
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure GPIO pin output Level
    HAL_GPIO_WritePin(LED_GPIO_BANK, LED_GPIO_PIN, GPIO_PIN_RESET);

    // Configure GPIO pin : PA5 - Pin under test
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin   = LED_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LED_GPIO_BANK, &GPIO_InitStruct);
}


/**
 * @brief Function implementing the LED-flash task thread.
 *
 * @param  argument: Not used.
 */
static void task_led(void *argument) {
    
    uint32_t last_tick = 0;

    // The task's main loop
    while (1) {
        // Periodically update the display and flash the USER LED
        // Get the ms timer value
        uint32_t tick = HAL_GetTick();
        if (tick - last_tick > LED_PAUSE_MS) {
            last_tick = tick;
            HAL_GPIO_WritePin(LED_GPIO_BANK, LED_GPIO_PIN, GPIO_PIN_SET);
        } else if (tick - last_tick > LED_PULSE_MS) {
            HAL_GPIO_WritePin(LED_GPIO_BANK, LED_GPIO_PIN, GPIO_PIN_RESET);
        }
        
        // End of cycle delay
        osDelay(10);
    }
}


/**
 * @brief Function implementing the FreeRTOS HTTP task thread.
 *
 * @param  argument: Not used.
*/
static void task_http(void *argument) {
    
    uint32_t ping_count = 1;
    uint32_t kill_time = 0;
    uint32_t send_tick = 0;
    bool do_close_channel = false;
    enum MvStatus result = MV_STATUS_OKAY;

    // Set up channel notifications
    http_notification_center_setup();

    // Run the thread's main loop
    while (1) {
        uint32_t tick = HAL_GetTick();
        if (tick - send_tick > REQUEST_SEND_PERIOD_MS) {
            // Display the current count
            send_tick = tick;
            server_log("Ping %lu", ping_count++);

            // No channel open?
            if (http_handles.channel == 0 && http_open_channel()) {
                result = http_send_request(ping_count);
                if (result != 0) do_close_channel = true;
                kill_time = tick;
            } else {
                server_error("Channel handle not zero");
                if (http_handles.channel != 0) do_close_channel = true;
            }
        }

        // Process a request's response if indicated by the ISR
        if (received_request) process_http_response();
        
        // Respond to unexpected channel closure
        if (channel_was_closed) {
            enum MvClosureReason reason = 0;
            if (mvGetChannelClosureReason(http_handles.channel, &reason) == MV_STATUS_OKAY) {
                server_log("Closure reason: %lu", (uint32_t)reason);
            }
            
            channel_was_closed = false;
            do_close_channel = true;
        }
        
        // Use 'kill_time' to force-close an open HTTP channel
        // if it's been left open too long
        if (kill_time > 0 && tick - kill_time > CHANNEL_KILL_PERIOD_MS) {
            do_close_channel = true;
            server_error("HTTP request timed out");
        }

        // If we've received a response in an interrupt handler,
        // we can close the HTTP channel for the time being
        if (received_request || do_close_channel) {
            do_close_channel = false;
            received_request = false;
            kill_time = 0;
            http_close_channel();
        }
        
        // End of cycle delay
        osDelay(10);
    }
}


/**
 * @brief Process HTTP response data
 */
static void process_http_response(void) {
    
    // We have received data via the active HTTP channel so establish
    // an `MvHttpResponseData` record to hold response metadata
    static struct MvHttpResponseData resp_data;
    enum MvStatus status = mvReadHttpResponseData(http_handles.channel, &resp_data);
    if (status == MV_STATUS_OKAY) {
        // Check we successfully issued the request (`result` is OK) and
        // the request was successful (status code 200)
        if (resp_data.result == MV_HTTPRESULT_OK) {
            if (resp_data.status_code == 200) {
                server_log("HTTP response header count: %lu", resp_data.num_headers);
                server_log("HTTP response body length: %lu", resp_data.body_length);

                // Set up a buffer that we'll get Microvisor to write
                // the response body into
                uint8_t buffer[resp_data.body_length + 1];
                memset((void *)buffer, 0x00, resp_data.body_length + 1);
                status = mvReadHttpResponseBody(http_handles.channel, 0, buffer, resp_data.body_length);
                if (status == MV_STATUS_OKAY) {
                    // Retrieved the body data successfully so log it
                    server_log("Message JSON:\n%s", buffer);
                    output_headers(resp_data.num_headers > MAX_HEADERS_OUTPUT ? MAX_HEADERS_OUTPUT : resp_data.num_headers);
                } else {
                    server_error("HTTP response body read status %i", status);
                }
            } else {
                server_error("HTTP status code: %lu", resp_data.status_code);
            }
        } else {
            server_error("Request failed. Status: %i", resp_data.result);;
        }
    } else {
        server_error("Response data read failed. Status: %i", status);
    }
}


/**
 * @brief Output all received headers.
 *
 * @param n: The number of headers to list.
 */
static void output_headers(uint32_t n) {
    
    if (n > 0) {
        enum MvStatus status = MV_STATUS_OKAY;
        uint8_t buffer[256];
        for (uint32_t i = 0 ; i < n ; ++i) {
            memset((void *)buffer, 0x00, 256);
            status = mvReadHttpResponseHeader(http_handles.channel, i, buffer, 255);
            if (status == MV_STATUS_OKAY) {
                server_log("Header %lu. %s", i + 1, buffer);
            } else {
                server_error("Could not read header %lu", i + 1);
            }
        }
    }
}


/**
 * @brief Show basic device info.
 */
static void log_device_info(void) {
    
    uint8_t buffer[35] = { 0 };
    mvGetDeviceId(buffer, 34);
    server_log("Device: %s", buffer);
    server_log("   App: %s %s", APP_NAME, APP_VERSION);
    server_log(" Build: %i", BUILD_NUM);
}
