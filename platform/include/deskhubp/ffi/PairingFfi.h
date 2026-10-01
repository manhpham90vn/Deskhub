#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DH_PAIRING_INVITE_CAP 256
#define DH_QR_MAX_SIZE 177

int dh_pairing_invite(uint16_t port, const char* bind_ip, char* out, int capacity);

void dh_pairing_revoke(void);

int64_t dh_pairing_token_ttl_seconds(void);

int dh_qr_encode(const char* text, uint8_t* modules, int capacity);

int dh_pairing_invite_address(const char* invite, char* out, int capacity);

int dh_pairing_invite_new_host_key(const char* invite, char* out, int capacity);

#ifdef __cplusplus
}
#endif
