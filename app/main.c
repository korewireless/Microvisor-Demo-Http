/**
 *
 * Microvisor HTTP Communications Demo
 *
 * Copyright © 2024, KORE Wireless
 * Licence: MIT
 *
 */
#include "main.h"


/*
 * STATIC PROTOTYPES
 */
static void gpio_init(void);
static void start_app(void);
static void task_led(void *argument);
static void task_http(void *argument);
static void process_http_response(void);
static void output_headers(uint32_t n);
static void setup_sys_notification_center(void);
static void do_polite_deploy(void *arg);
static void do_clear_led(void* arg);


/*
 * GLOBALS
 *
 * Theses variables may be changed by interrupt handler code,
 * so we mark them as `volatile` to ensure compiler optimization
 * doesn't render them immutable at runtime
 */
volatile bool   received_request = false;
volatile bool   channel_was_closed = false;
volatile bool   polite_deploy = false;
         bool   reset_count = false;

// Central store for HTTP request management notification records.
// Holds HTTP_NT_BUFFER_SIZE_R records at a time -- each record is 16 bytes in size.
static struct MvNotification sys_notification_center[4] __attribute__((aligned(8)));
// Modified in ISR
static volatile uint32_t current_notification_index = 0;

// Local notification center/emitter handles
MvNotificationHandle sys_nc_handle;
MvSystemEventHandle sys_emitter_handle;


/**
 *  @brief The application entry point.
 */
int main(void) {

    // Reset of all peripherals, Initializes the Flash interface and the sys tick.
    HAL_Init();

    // Configure the system clock
    system_clock_config();

    // Initialize peripherals
    gpio_init();
    control_system_led(true);

    // Get the Device ID and build number
    log_device_info();

    // What happened before?
    show_wake_reason();

    // Configure system-level notification
    setup_sys_notification_center();

    // Start the network
    net_open_network();

    // Set up and start application threads
    start_app();

    // We should never get here as control is now taken by the FreeRTOS scheduler,
    // but just in case...
    while (1) {
        // NOP
    }
}


/**
 * @brief Initialize the MCU GPIO.
 *
 * Used to flash the Nucleo's USER LED, which is on GPIO Pin PA5.
 */
static void gpio_init(void) {

    // Enable GPIO port clock
    __HAL_RCC_GPIOA_CLK_ENABLE()

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
 * @brief Initialize the app's FreeRTOS threads and start the scheduler.
 */
static void start_app(void) {

    // This is the CMSIS/FreeRTOS thread task that flashed the USER LED
    const osThreadAttr_t attributes_thread_led = {
        .name = "LEDTask",
        .stack_size = 2048,
        .priority = osPriorityNormal
    };

    // This is the CMSIS/FreeRTOS thread task that sends HTTP requests
    const osThreadAttr_t attributes_thread_http = {
        .name = "HTTPTask",
        .stack_size = 16384,
        .priority = osPriorityNormal
    };

    // Init scheduler
    osKernelInitialize();

    // Create the FreeRTOS thread(s)
    osThreadNew(task_http, NULL, &attributes_thread_http);
    osThreadNew(task_led,  NULL, &attributes_thread_led);

    // Start the scheduler
    osKernelStart();
}


/**
 * @brief Function implementing the LED-flash task thread.
 *
 * @param argument: Not used.
 */
static void task_led(void* argument) {

    uint32_t last_tick = 0;
    osTimerId_t polite_timer = NULL;

    // FROM 3.3.0
    // Set a timer to turn off the System LED approx. 60s after boot
    osTimerId_t sys_led_timer = osTimerNew(do_clear_led, osTimerOnce, NULL, NULL);
    if (sys_led_timer != NULL) osTimerStart(sys_led_timer, 59 * 1000);

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

        // FROM 3.2.0
        // Check if the polite deployment flag has been set
        // via a Microvisor system notification
        if (polite_deploy) {
            server_log("Polite deployment notification issued");
            polite_deploy = false;

            // Set up a 30s timer to trigger the update
            // NOTE In a real-world application, you would apply the update
            //      see `do_polite_deploy()` as soon as any current critical task
            //      completes. Here we just demo the process using a HAL timer.
            polite_timer = osTimerNew(do_polite_deploy, osTimerOnce, NULL, NULL);
            const uint32_t timer_delay_s = 30;
            if (polite_timer != NULL && osTimerStart(polite_timer, timer_delay_s * 1000) == osOK) {
                server_log("Update will install in %lu seconds", timer_delay_s);
            }
        }

        // End of cycle delay
        osDelay(10);
    }
}


/**
 * @brief Function implementing the FreeRTOS HTTP task thread.
 *
 * @param argument: Not used.
*/
static void task_http(void *argument) {

    uint32_t ping_count = 1;
    uint32_t kill_time = 0;
    uint32_t send_tick = HAL_GetTick() - REQUEST_SEND_PERIOD_MS;
    bool do_close_channel = false;

    // Set up HTTP notifications
    http_setup_notification_center();

    // Run the thread's main loop
    while (1) {
        MvChannelHandle channel_handle = http_get_handle();
        uint32_t tick = HAL_GetTick();
        if (tick - send_tick > REQUEST_SEND_PERIOD_MS) {
            // Display the current count
            send_tick = tick;
            server_log("Ping %lu", ping_count++);

            // No channel open? The try and open a new one
            if (channel_handle == 0 && http_open_channel()) {
                if (http_send_request(ping_count) != 0) do_close_channel = true;
                kill_time = tick;
            } else {
                server_error("Channel handle not zero");
                if (channel_handle != 0) do_close_channel = true;
            }
        }

        // Process a request's response if indicated by the ISR
        if (received_request) process_http_response();

        // Respond to unexpected channel closure
        if (channel_was_closed) {
            enum MvClosureReason reason = 0;
            if (mvGetChannelClosureReason(channel_handle, &reason) == MV_STATUS_OKAY) {
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

        // Reached the end of the items available from the API
        // so reset the counter and start again
        if (reset_count) {
            reset_count = false;
            ping_count = 1;
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
    MvChannelHandle channel_handle = http_get_handle();
    enum MvStatus status = mvReadHttpResponseData(channel_handle, &resp_data);
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
                status = mvReadHttpResponseBody(channel_handle, 0, buffer, resp_data.body_length);
                if (status == MV_STATUS_OKAY) {
                    // Retrieved the body data successfully so log it
                    server_log("Message JSON:\n%s", buffer);
                    output_headers(resp_data.num_headers > MAX_HEADERS_OUTPUT ? MAX_HEADERS_OUTPUT : resp_data.num_headers);
                } else {
                    server_error("HTTP response body read status %i", status);
                }
            } else if (resp_data.status_code == 404) {
                // Reached the end of available items, so reset the counter
                reset_count = true;
                server_log("Resetting ping count");
            } else {
                server_error("HTTP status code: %lu", resp_data.status_code);
            }
        } else {
            server_error("Request failed. Status: %i", resp_data.result);
        }
    } else {
        server_error("Response data read failed. Status: %i", status);
    }
}


/**
 * @brief Output all received headers.
 *
 * @param num_headers: The number of headers to list.
 */
static void output_headers(uint32_t num_headers) {

    static uint8_t buffer[256] = {0};

    if (num_headers > 0) {
        for (uint32_t i = 0 ; i < num_headers ; ++i) {
            memset((void *)buffer, 0x00, 256);
            if (mvReadHttpResponseHeader(http_get_handle(), i, buffer, 255) == MV_STATUS_OKAY) {
                server_log("Header %02lu. %s", i + 1, buffer);
            } else {
                server_error("Could not read header %lu", i + 1);
            }
        }
    }
}


/**
 * @brief Configure the System Notification Center.
 */
static void setup_sys_notification_center(void) {

    // Clear the notification store
    memset((void*)sys_notification_center, 0x00, sizeof(sys_notification_center));

    // Configure a notification center for system notifications
    static struct MvNotificationSetup sys_notification_setup = {
        .irq = TIM1_BRK_IRQn,
        .buffer = sys_notification_center,
        .buffer_size = sizeof(sys_notification_center)
    };

    // Ask Microvisor to establish the notification center
    // and confirm that it has accepted the request
    enum MvStatus status = mvSetupNotifications(&sys_notification_setup, &sys_nc_handle);
    do_assert(status == MV_STATUS_OKAY, "Could not set up sys NC");
    server_log("System Notification Center handle: %lu", (uint32_t)sys_nc_handle);

    // Tell Microvisor to use the new notification center for system notifications
    const struct MvOpenSystemNotificationParams sys_notification_params = {
        .notification_handle = sys_nc_handle,
        .notification_tag = 0,
        .notification_source = MV_SYSTEMNOTIFICATIONSOURCE_UPDATE
    };

    status = mvOpenSystemNotification(&sys_notification_params, &sys_emitter_handle);
    do_assert(status == MV_STATUS_OKAY, "Could not enable system notifications");

    // Start the notification IRQ
    NVIC_ClearPendingIRQ(TIM1_BRK_IRQn);
    NVIC_EnableIRQ(TIM1_BRK_IRQn);
}


/**
 * @brief A CMSIS/FreeRTOS timer callback function.
 *
 * This is called when the polite deployment timer (see `task_led()`) fires.
 * It tells Microvisor to apply the application update that has previously
 * been signalled as ready to be deployed.
 *
 * `mvRestart()` should cause the application to be torn down and restarted,
 * but it's important to check the returned value in case Microvisor was not
 * able to perform the restart for some reason.
 *
 * @param arg: Pointer to and argument value passed by the timer controller.
 *             Unused here.
 */
static void do_polite_deploy(void* arg) {

    enum MvStatus status = mvRestart(MV_RESTARTMODE_AUTOAPPLYUPDATE);
    if (status != MV_STATUS_OKAY) {
        server_error("Could not apply update (%lu)", (uint32_t)status);
    }
}


/**
 * @brief A CMSIS/FreeRTOS timer callback function.
 *
 * This is called when the sys led timer (see `task_led()`) fires.
 * It tells Microvisor to disable the system LED
 *
 * @param arg: Pointer to and argument value passed by the timer controller.
 *             Unused here.
 */
static void do_clear_led(void* arg) {

    control_system_led(false);
    server_log("System LED disabled");
}


/**
 * @brief The System Notification Center interrupt handler.
 *
 * This is called by Microvisor -- we need to check for key events
 * such as polite deployment.
 */
void TIM1_BRK_IRQHandler(void) {

    // Check for a suitable event: readable data in the channel
    bool got_notification = false;
    struct MvNotification notification = sys_notification_center[current_notification_index];
    if (notification.event_type == MV_EVENTTYPE_UPDATEDOWNLOADED) {
        // Flag we need to access received data and to close the HTTP channel
        // when we're back in the main loop. This lets us exit the ISR quickly.
        // We should not make Microvisor System Calls in the ISR.
        polite_deploy = true;
        got_notification = true;
    }

    if (got_notification) {
        // Point to the next record to be written
        current_notification_index = (current_notification_index + 1) % HTTP_NT_BUFFER_SIZE_R;

        // Clear the current notifications event
        // See https://www.twilio.com/docs/iot/microvisor/microvisor-notifications#buffer-overruns
        notification.event_type = 0;
    }
}

