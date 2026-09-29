#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DH_CLIENT_KEY_NAME_CAP 72
#define DH_CLIENT_KEY_FINGERPRINT_CAP 64
#define DH_PUBLIC_KEY_TEXT_CAP 1024

typedef enum {
    DHClientKeyOk = 0,
    DHClientKeyInvalidName = 1,
    DHClientKeyNameInUse = 2,
    DHClientKeyUnreadable = 3,
    DHClientKeyWriteFailed = 4,
    DHClientKeyDefaultKey = 5,
    DHClientKeyInUse = 6,
    DHClientKeyMissing = 7,
} DHClientKeyError;

typedef struct {
    char name[DH_CLIENT_KEY_NAME_CAP];
    char fingerprint[DH_CLIENT_KEY_FINGERPRINT_CAP];
} DHClientKey;

int dh_client_keys(DHClientKey* out, int capacity);
int dh_client_public_key(const char* name, char* out, int capacity);
DHClientKeyError dh_client_key_generate(const char* name);
DHClientKeyError dh_client_key_delete(const char* name);
DHClientKeyError dh_client_key_import(const char* name, const char* private_key,
    const char* passphrase);
const char* dh_client_key_error_text(DHClientKeyError error);

#ifdef __cplusplus
}
#endif
