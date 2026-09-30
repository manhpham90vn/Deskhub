#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DH_HOST_NAME_CAP 72
#define DH_HOST_ENDPOINT_CAP 64
#define DH_HOST_FINGERPRINT_CAP 64
#define DH_HOST_PROFILES_UNREADABLE (-1)

typedef enum {
    DHHostProfileOk = 0,
    DHHostProfileInvalidAlias = 1,
    DHHostProfileInvalidAddress = 2,
    DHHostProfileMissingHostKey = 3,
    DHHostProfileInvalidHostKey = 4,
    DHHostProfileAliasExists = 6,
    DHHostProfileAliasMissing = 7,
    DHHostProfileAliasAmbiguous = 8,
    DHHostProfileStoreUnreadable = 10,
    DHHostProfileStoreFull = 11,
    DHHostProfileWriteFailed = 12,
    DHHostProfileKeyExists = 13,
} DHHostProfileError;

typedef struct {
    char alias[DH_HOST_NAME_CAP];
    char endpoint[DH_HOST_ENDPOINT_CAP];
    char fingerprint[DH_HOST_FINGERPRINT_CAP];
} DHHostProfile;

int dh_host_profiles(DHHostProfile* out, int capacity);
DHHostProfileError dh_host_profile_remove(const char* alias);
DHHostProfileError dh_host_trust_new(const char* address, const char* fingerprint);
const char* dh_host_profile_error_text(DHHostProfileError error);
int dh_trust_new_host_prompt(const char* address, const char* fingerprint, char* out,
    int capacity);

#ifdef __cplusplus
}
#endif
