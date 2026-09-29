#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DH_ADDR_CAP 64
#define DH_DEVICE_NAME_CAP 72

typedef struct {
    char addr[DH_ADDR_CAP];
    char name[DH_DEVICE_NAME_CAP];
    char lastConnected[24];
} DHDeviceRow;

int dh_device_rows(DHDeviceRow* out, int capacity);

typedef struct {
    char name[80];
    char shortKey[16];
    char fingerprint[64];
} DHPairedDevice;

int dh_paired_devices(DHPairedDevice* out, int capacity);
bool dh_paired_add_public_key(const char* public_key);
bool dh_paired_forget(const char* fingerprint);
void dh_paired_forget_all(void);
int dh_host_fingerprint(char* out, int capacity);

#ifdef __cplusplus
}
#endif
