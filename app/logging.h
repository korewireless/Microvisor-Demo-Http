/**
 *
 * Microvisor HTTP Communications Demo
 * Version 3.1.0
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#ifndef LOGGING_H
#define LOGGING_H


/*
 * CONSTANTS
 */
#define     USER_TAG_LOGGING_REQUEST_NETWORK    1
#define     USER_TAG_LOGGING_OPEN_CHANNEL       2
#define     USER_TAG_HTTP_OPEN_CHANNEL          3

#define     USER_HANDLE_LOGGING_STARTED         0xFFFF
#define     USER_HANDLE_LOGGING_OFF             0

#define     LOG_MESSAGE_MAX_LEN_B               1024
#define     LOG_BUFFER_SIZE_B                   4096

#define     NET_NC_BUFFER_SIZE_R                8


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void            server_log(char* format_string, ...);
void            server_error(char* format_string, ...);
void            do_assert(bool condition, char* message);


#ifdef __cplusplus
}
#endif


#endif /* LOGGING_H */
