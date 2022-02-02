/**
 *
 * Microvisor HTTP Communications Demo
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: MIT
 *
 */
#ifndef LOGGING_H
#define LOGGING_H


#define     USER_TAG_LOGGING_REQUEST_NETWORK    1
#define     USER_TAG_LOGGING_OPEN_CHANNEL       2
#define     USER_TAG_HTTP_OPEN_CHANNEL          3

#ifdef __cplusplus
extern "C" {
#endif


void log_open_channel(void);
void log_close_channel(void);
MvNetworkHandle get_net_handle();


#ifdef __cplusplus
}
#endif


#endif /* LOGGING_H */
