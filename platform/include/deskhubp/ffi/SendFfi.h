#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DHSend DHSend;

typedef enum {
    DHSendIdle = 0,
    DHSendConnecting = 1,
    DHSendSending = 2,
    DHSendDone = 3,
    DHSendRefused = 4,
    DHSendFailed = 5,
} DHSendState;

typedef struct {
    int32_t state;
    bool finished;
    uint16_t fileIndex;
    uint16_t fileCount;
    uint64_t bytes;
    uint64_t total;
    char name[256];
    char message[256];
} DHSendProgress;

int dh_send_check(const char* const* paths, int count, char* error, int errorCapacity);

DHSend* dh_send_start(const char* address, const char* name, const char* const* paths,
    int count);

void dh_send_snapshot(DHSend* handle, DHSendProgress* out);

void dh_send_cancel(DHSend* handle);

void dh_send_stop(DHSend* handle);

#ifdef __cplusplus
}
#endif
