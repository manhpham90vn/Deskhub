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
int dh_host_public_key(char* out, int capacity);

typedef struct {
    char name[80];
    char address[64];
    char shortKey[16];
    char fingerprint[64];
    char requestedAt[24];
} DHAccessRequest;

int dh_access_requests(DHAccessRequest* out, int capacity);
bool dh_access_approve(const char* fingerprint);
bool dh_access_deny(const char* fingerprint);
uint64_t dh_access_requests_generation(void);

#ifdef __cplusplus
}
#endif
