/**
 *
 * Microvisor HTTP Communications Demo
 * Version 1.0.1
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
#include <unistd.h>
#include <errno.h>

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
#define     LED_GPIO_BANK           GPIOA
#define     LED_GPIO_PIN            GPIO_PIN_5
#define     LED_FLASH_PERIOD        1000

#define     DEBUG_TASK_PAUSE        1000
#define     DEFAULT_TASK_PAUSE      500

#define     REQUEST_SEND_PERIOD     30000
#define     CHANNEL_KILL_PERIOD     15000


/*
 * PROTOTYPES
 */
void        system_clock_config(void);
void        gpio_init(void);
void        start_led_task(void *argument);
void        start_http_task(void *argument);

void        http_channel_center_setup(void);
void        http_open_channel(void);
void        http_close_channel(void);
bool        http_send_request();
void        http_process_response(void);

void        log_device_info(void);

void        output_headers(uint32_t n);


#ifdef __cplusplus
}
#endif


#endif      // _MAIN_H_
