/**
 *
 * Microvisor HTTP Communications Demo
 * Version 1.0.1
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


struct {
    MvNotificationHandle notification;
    MvNetworkHandle network;
    MvChannelHandle channel;
} log_handles = { 0, 0, 0 };

static volatile struct MvNotification log_notification_buffer[16];


void log_open_channel(void) {
    memset((void *)log_notification_buffer, 0xff, sizeof(log_notification_buffer));
    static struct MvNotificationSetup log_notification_setup = {
        .irq = TIM1_BRK_IRQn,
        .buffer = (struct MvNotification *)log_notification_buffer,
        .buffer_size = sizeof(log_notification_buffer)
    };

    uint32_t status = mvSetupNotifications(&log_notification_setup, &log_handles.notification);
    assert(status == MV_STATUS_OKAY);

    NVIC_ClearPendingIRQ(TIM1_BRK_IRQn);
    NVIC_EnableIRQ(TIM1_BRK_IRQn);

    struct MvRequestNetworkParams nw_params = {
        .version = 1,
        .v1 = {
            .notification_handle = log_handles.notification,
            .notification_tag = USER_TAG_LOGGING_REQUEST_NETWORK,
        }
    };

    status = mvRequestNetwork(&nw_params, &log_handles.network);
    assert(status == MV_STATUS_OKAY);

    static volatile uint8_t receive_buffer[16];
    static volatile uint8_t send_buffer[512] __attribute__((aligned(512)));
    char endpoint[] = "log";
    struct MvOpenChannelParams channel_config = {
        .version = 1,
        .v1 = {
            .notification_handle = log_handles.notification,
            .notification_tag = USER_TAG_LOGGING_OPEN_CHANNEL,
            .network_handle = log_handles.network,
            .receive_buffer = (uint8_t*)receive_buffer,
            .receive_buffer_len = sizeof(receive_buffer),
            .send_buffer = (uint8_t*)send_buffer,
            .send_buffer_len = sizeof(send_buffer),
            .channel_type = MV_CHANNELTYPE_OPAQUEBYTES,
            .endpoint = (uint8_t*)endpoint,
            .endpoint_len = strlen(endpoint)
        }
    };

    while (1) {  // busy wait for connection
        enum MvNetworkStatus status;
        if (mvGetNetworkStatus(log_handles.network, &status) == MV_STATUS_OKAY &&
            status == MV_NETWORKSTATUS_CONNECTED) {
            break;
        }

        for (volatile unsigned i = 0; i < 50000; i++) {} // busy delay
    }

    status = mvOpenChannel(&channel_config, &log_handles.channel);
    assert(status == MV_STATUS_OKAY);
}


void log_close_channel(void) {
    if (log_handles.channel != 0) {
        uint32_t status = mvCloseChannel(&log_handles.channel);
        assert(status == MV_STATUS_OKAY);
    }

    assert(log_handles.channel == 0);
    if (log_handles.network != 0) {
        uint32_t status = mvReleaseNetwork(&log_handles.network);
        assert(status == MV_STATUS_OKAY);
    }

    assert(log_handles.network == 0);
    if (log_handles.notification != 0) {
        uint32_t status = mvCloseNotifications(&log_handles.notification);
        assert(status == MV_STATUS_OKAY);
    }

    assert(log_handles.notification == 0);

    NVIC_DisableIRQ(TIM1_BRK_IRQn);
    NVIC_ClearPendingIRQ(TIM1_BRK_IRQn);
}


/**
 *  Return the network handle to the caller.
 *
 *  @returns The network handle (uint32_t).
 *
 */
MvNetworkHandle get_net_handle() {
    return log_handles.network;
}


void TIM1_BRK_IRQHandler(void) {
    // Catch network and channel events here
}


/**
 *  Wire up stdio syscall, so printf() works
 *  as a logging source
 *
 */
int _write(int file, char *ptr, int len) {
    if (file != STDOUT_FILENO) {
        errno = EBADF;
        return -1;
    }

    if (log_handles.channel == 0) {
        log_open_channel();
    }

    uint32_t written, status;
    status = mvWriteChannelStream(log_handles.channel, (const uint8_t*)ptr, len, &written);
    if (status == MV_STATUS_OKAY) {
        return written;
    } else {
        errno = EIO;
        return -1;
    }
}
