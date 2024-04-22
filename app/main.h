/**
 *
 * Microvisor HTTP Communications Demo
 *
 * Copyright © 2024, KORE Wireless
 * Licence: MIT
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
#include "uart_logging.h"
#include "http.h"
#include "network.h"


/*
 * CONSTANTS
 */
#define     LED_GPIO_BANK               GPIOA
#define     LED_GPIO_PIN                GPIO_PIN_5

#define     DEBUG_TASK_PAUSE_MS         1000
#define     DEFAULT_TASK_PAUSE_MS       500
#define     LED_PAUSE_MS                2000
#define     LED_PULSE_MS                100

#define     REQUEST_SEND_PERIOD_MS      30000
#define     CHANNEL_KILL_PERIOD_MS      15000

#define     MAX_HEADERS_OUTPUT          10


#endif      // _MAIN_H_
