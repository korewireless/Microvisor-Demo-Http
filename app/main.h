/**
 *
 * Microvisor HTTP Communications Demo
 * Version 1.3.3
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _MAIN_H_
#define _MAIN_H_


/*
 * INCLUDES
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "cmsis_os.h"
#include "mv_syscalls.h"

// App includes
#include "logging.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * CONSTANTS
 */
#define     LED_GPIO_BANK               GPIOA
#define     LED_GPIO_PIN                GPIO_PIN_5

#define     DEBUG_TASK_PAUSE_MS         1000
#define     DEFAULT_TASK_PAUSE_MS       500

#define     REQUEST_SEND_PERIOD_MS      30000
#define     CHANNEL_KILL_PERIOD_MS      15000

#define     HTTP_RX_BUFFER_SIZE_B       1536
#define     HTTP_TX_BUFFER_SIZE_B       512


/*
 * PROTOTYPES
 */
void        system_clock_config(void);
void        gpio_init(void);
void        start_led_task(void *argument);
void        start_http_task(void *argument);

void        http_channel_center_setup(void);
bool        http_open_channel(void);
void        http_close_channel(void);
bool        http_send_request();
void        http_process_response(void);

void        log_device_info(void);
void        server_log(char* format_string, ...);
void        server_error(char* format_string, ...);

void        output_headers(uint32_t n);


#ifdef __cplusplus
}
#endif


#endif      // _MAIN_H_
