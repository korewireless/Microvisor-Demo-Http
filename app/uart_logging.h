/**
 *
 * Microvisor HTTP Communications Demo
 * Version 2.0.7
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef UART_LOGGING_H
#define UART_LOGGING_H


#ifdef __cplusplus
extern "C" {
#endif


bool    UART_init(void);
void    UART_output(uint8_t* buffer, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif
