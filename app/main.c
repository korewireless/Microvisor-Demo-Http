/**
 *
 * Microvisor HTTP Communications Demo
 * Version 1.0.1
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


/**
 *  GLOBALS
 */

// This is the FreeRTOS thread task that flashed the USER LED
osThreadId_t LEDTask;
const osThreadAttr_t led_task_attributes = {
    .name = "LEDTask",
    .stack_size = 512,
    .priority = (osPriority_t) osPriorityNormal
};

// This is the FreeRTOS thread task that sends HTTP requests
osThreadId_t HTTPTask;
const osThreadAttr_t http_task_attributes = {
    .name = "HTTPTask",
    .stack_size = 1024,
    .priority = (osPriority_t) osPriorityNormal
};

// Central store for Microvisor resource handles used in this code.
// See `https://www.twilio.com/docs/iot/microvisor/syscalls#http_handles`
struct {
    MvNotificationHandle notification;
    MvNetworkHandle      network;
    MvChannelHandle      channel;
} http_handles = { 0, 0, 0 };

/**
 *  Theses variables may be changed by interrupt handler code,
 *  so we mark them as `volatile` to ensure compiler optimization
 *  doesn't render them immutable at runtime
 */
volatile bool request_recv = false;

// Central store for notification records.
// Holds one record at a time -- each record is 16 bytes.
volatile struct MvNotification http_notification_center[16];


/**
 *  @brief The application entry point.
 */
int main(void) {
    // Reset of all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    // Configure the system clock
    system_clock_config();

    // Initialize peripherals
    gpio_init();

    // Init scheduler
    osKernelInitialize();

    // Create the FreeRTOS thread(s)
    HTTPTask = osThreadNew(start_http_task, NULL, &http_task_attributes);
    LEDTask  = osThreadNew(start_led_task,  NULL, &led_task_attributes);

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
uint32_t SECURE_SystemCoreClockUpdate() {
    uint32_t clock = 0;
    mvGetHClk(&clock);
    return clock;
}


/**
  * @brief System clock configuration.
  */
void system_clock_config(void) {
    SystemCoreClockUpdate();
    HAL_InitTick(TICK_INT_PRIORITY);
}


/**
  * @brief Initialize the MCU GPIO.
  *
  * Used to flash the Nucleo's USER LED, which is on GPIO Pin PA5.
  */
void gpio_init(void) {
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
void start_led_task(void *argument) {
    uint32_t last_tick = 0;

    // The task's main loop
    while (true) {
        // Periodically update the display and flash the USER LED
        // Get the ms timer value
        uint32_t tick = HAL_GetTick();
        if (tick - last_tick > DEFAULT_TASK_PAUSE) {
            last_tick = tick;

            // Toggle the USER LED's GPIO pin
            HAL_GPIO_TogglePin(LED_GPIO_BANK, LED_GPIO_PIN);
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
void start_http_task(void *argument) {
    uint32_t ping_count = 1;
    uint32_t kill_time = 0;
    uint32_t send_tick = 0;
    bool close_channel = false;

    // Get the Device ID and build number and log them
    log_device_info();

    // Set up channel notifications
    http_channel_center_setup();

    // Run the thread's main loop
    while (true) {
        uint32_t tick = HAL_GetTick();
        if (tick - send_tick > REQUEST_SEND_PERIOD) {
            // Display the current count
            send_tick = tick;
            printf("[DEBUG] Ping %lu\n", ping_count++);

            // No channel open? Try and send the temperature
            if (http_handles.channel == 0) {
                http_open_channel();
                bool result = http_send_request();
                if (!result) close_channel = true;
                kill_time = tick;
            } else {
                printf("[ERROR] Channel handle not zero\n");
            }
        }

        // Process a request's response if indicated by the ISR
        if (request_recv) {
            http_process_response();
        }

        // Use 'kill_time' to force-close an open HTTP channel
        // if it's been left open too long
        if (kill_time > 0 && tick - kill_time > CHANNEL_KILL_PERIOD) {
            close_channel = true;
        }

        // If we've received a response in an interrupt handler,
        // we can close the HTTP channel for the time being
        if (request_recv || close_channel) {
            close_channel = false;
            request_recv = false;
            kill_time = 0;
            http_close_channel();
        }

        // Pause per cycle
        osDelay(10);
    }
}


/**
 *  @brief Open a new HTTP channel.
 */
void http_open_channel(void) {
    // Set up the HTTP channel's multi-use send and receive buffers
    static volatile uint8_t http_rx_buffer[1536] __attribute__((aligned(512)));
    static volatile uint8_t http_tx_buffer[512] __attribute__((aligned(512)));
    const char endpoint[] = "";

    // Get the network channel handle.
    // NOTE This is set in `logging.c` which puts the network in place
    //      (ie. so the network handle != 0) well in advance of this being called
    http_handles.network = get_net_handle();
    assert(http_handles.network != 0);
    printf("[DEBUG] Network handle: %lu\n", (uint32_t)http_handles.network);

    // Configure the required data channel
    struct MvOpenChannelParams channel_config = {
        .version = 1,
        .v1 = {
            .notification_handle = http_handles.notification,
            .notification_tag    = USER_TAG_HTTP_OPEN_CHANNEL,
            .network_handle      = http_handles.network,
            .receive_buffer      = (uint8_t*)http_rx_buffer,
            .receive_buffer_len  = sizeof(http_rx_buffer),
            .send_buffer         = (uint8_t*)http_tx_buffer,
            .send_buffer_len     = sizeof(http_tx_buffer),
            .channel_type        = MV_CHANNELTYPE_HTTP,
            .endpoint            = (uint8_t*)endpoint,
            .endpoint_len        = 0
        }
    };

    // Ask Microvisor to open the channel
    // and confirm that it has accepted the request
    enum MvStatus status = mvOpenChannel(&channel_config, &http_handles.channel);
    if (status == MV_STATUS_OKAY) {
        printf("[DEBUG] HTTP channel handle: %lu\n", (uint32_t)http_handles.channel);
        assert(http_handles.channel != 0);
    } else {
        printf("[ERROR] HTTP channel opening failed. Status: %i\n", status);
    }
}


/**
 *  @brief Close the currently open HTTP channel.
 */
void http_close_channel(void) {
    // If we have a valid channel handle -- ie. it is non-zero --
    // then ask Microvisor to close it and confirm acceptance of
    // the closure request.
    if (http_handles.channel != 0) {
        enum MvStatus status = mvCloseChannel(&http_handles.channel);
        printf("[DEBUG] HTTP channel closed\n");
        assert(status == MV_STATUS_OKAY);
    }

    // Confirm the channel handle has been invalidated by Microvisor
    assert(http_handles.channel == 0);
}


/**
 * @brief   Configure the channel Notification Center.
 */
void http_channel_center_setup(void) {
    // Clear the notification store
    memset((void *)http_notification_center, 0xFF, sizeof(http_notification_center));

    // Configure a notification center for network-centric notifications
    static struct MvNotificationSetup http_notification_setup = {
        .irq = TIM8_BRK_IRQn,
        .buffer = (struct MvNotification *)http_notification_center,
        .buffer_size = sizeof(http_notification_center)
    };

    // Ask Microvisor to establish the notification center
    // and confirm that it has accepted the request
    uint32_t status = mvSetupNotifications(&http_notification_setup, &http_handles.notification);
    assert(status == MV_STATUS_OKAY);

    // Start the notification IRQ
    NVIC_ClearPendingIRQ(TIM8_BRK_IRQn);
    NVIC_EnableIRQ(TIM8_BRK_IRQn);
    printf("[DEBUG] Notification center handle: %lu\n", (uint32_t)http_handles.notification);
}


/**
 * @brief Send a stock HTTP request.
 *
 * @returns `true` if the request was accepted by Microvisor, otherwise `false`
 */
bool http_send_request() {
    // Make sure we have a valid channel handle
    if (http_handles.channel != 0) {
        printf("[DEBUG] Sending HTTP request\n");

        // Set up the request
        const char verb[] = "GET";
        const char uri[] = "https://jsonplaceholder.typicode.com/todos/1";
        const char body[] = "";
        struct MvHttpHeader hdrs[] = {};
        struct MvHttpRequest request_config = {
            .method = (uint8_t *)verb,
            .method_len = strlen(verb),
            .url = (uint8_t *)uri,
            .url_len = strlen(uri),
            .num_headers = 0,
            .headers = hdrs,
            .body = (uint8_t *)body,
            .body_len = strlen(body),
            .timeout_ms = 10000
        };

        // Issue the request -- and check its status
        enum MvStatus status = mvSendHttpRequest(http_handles.channel, &request_config);
        if (status == MV_STATUS_OKAY) {
            printf("[DEBUG] Request sent to Twilio\n");
            return true;
        }

        // Report send failure
        printf("[ERROR] Could not issue request. Status: %i\n", status);
        return false;
    }

    // There's no open channel, so open open one now and
    // try to send again
    http_open_channel();
    return http_send_request();
}


/**
 *  @brief The HTTP channel notification interrupt handler.
 *
 *  This is called by Microvisor -- we need to check for key events
 *  and extract HTTP response data when it is available.
 */
void TIM8_BRK_IRQHandler(void) {
    // Get the event type
    enum MvEventType event_kind = http_notification_center->event_type;
    printf("[DEBUG] Channel notification center IRQ called for event: %u\n", event_kind);

    if (event_kind == MV_EVENTTYPE_CHANNELDATAREADABLE) {
        // Flag we need to access received data and to close the HTTP channel
        // when we're back in the main loop. This lets us exit the ISR quickly.
        // We should not make Microvisor System Calls in the ISR.
        request_recv = true;
    }
}


/**
 * @brief Process HTTP response data
 */
void http_process_response(void) {
    // We have received data via the active HTTP channel so establish
    // an `MvHttpResponseData` record to hold response metadata
    static struct MvHttpResponseData resp_data;
    enum MvStatus status = mvReadHttpResponseData(http_handles.channel, &resp_data);
    if (status == MV_STATUS_OKAY) {
        // Check we successfully issued the request (`result` is OK) and
        // the request was successful (status code 200)
        if (resp_data.result == MV_HTTPRESULT_OK) {
            if (resp_data.status_code == 200) {
                // Set up a buffer that we'll get Microvisor to write
                // the response body into
                uint8_t buffer[resp_data.body_length + 1];
                memset((void *)buffer, 0x00, resp_data.body_length + 1);
                status = mvReadHttpResponseBody(http_handles.channel, 0, buffer, resp_data.body_length);
                if (status == MV_STATUS_OKAY) {
                    // Retrieved the body data successfully so log it
                    printf("[DEBUG] HTTP response header count: %lu\n", resp_data.num_headers);
                    printf("[DEBUG] HTTP response body length: %lu\n", resp_data.body_length);
                    printf("%s\n", buffer);
                    output_headers(resp_data.num_headers);
                } else {
                    printf("[ERROR] HTTP response body read status %i\n", status);
                }
            } else {
                printf("[ERROR] HTTP status code: %lu\n", resp_data.status_code);
            }
        } else {
            printf("[ERROR] Request failed. Status: %lu\n", (uint32_t)resp_data.result);;
        }
    } else {
        printf("[ERROR] Response data read failed. Status: %i\n", status);
    }
}


/**
 * @brief Output all received headers.
 */
void output_headers(uint32_t n) {
    enum MvStatus status = 0;
    uint8_t buffer[256];
    for (uint32_t i = 0 ; i < n ; i++) {
        memset((void *)buffer, 0x00, 256);
        status = mvReadHttpResponseHeader(http_handles.channel, i, buffer, 255);
        if (status == MV_STATUS_OKAY) {
            printf("%lu. %s\n", i + 1, buffer);
        } else {
            printf("[ERROR] Could not read header %lu\n", i + 1);
        }
    }
}


/**
 * @brief Show basic device info.
 */
void log_device_info(void) {
    uint8_t buffer[35] = { 0 };
    mvGetDeviceId(buffer, 34);
    printf("Device: %s\n   App: %s %s\n Build: %i\n", buffer, APP_NAME, APP_VERSION, BUILD_NUM);
}