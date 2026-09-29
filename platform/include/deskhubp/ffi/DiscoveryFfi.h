#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DH_ADDR_CAP 64

typedef struct {
    char addr[DH_ADDR_CAP];
    char origin[20];
    char status[16];
    char ping[16];
    char lastConnected[24];
    bool known;
    bool online;
} DHDeviceRow;

typedef struct {
    uint32_t fps;
    uint32_t bitrateMbps;
    uint32_t maxDim;
    uint32_t port;
    bool allowInput;
    bool clientControl;
} DHUiSettings;

void dh_recent_touch(const char* address);

typedef enum {
    DHSettingsEntryArea = 0,
    DHSettingsEntryHint = 1,
    DHSettingsEntrySection = 2,
    DHSettingsEntrySetting = 3,
} DHSettingsEntryKind;

typedef enum {
    DHSettingNone = 0,
    DHSettingFps = 1,
    DHSettingBitrate = 2,
    DHSettingQuality = 3,
    DHSettingAllowInput = 4,
    DHSettingShareAudio = 5,
    DHSettingTransferFolder = 6,
    DHSettingAutoShare = 7,
    DHSettingPermissions = 8,
    DHSettingPlayAudio = 9,
    DHSettingPort = 10,
    DHSettingClipboardSync = 11,
    DHSettingKeepAwake = 12,
    DHSettingAutostart = 13,
    DHSettingCloseToTray = 14,
    DHSettingDeviceName = 15,
} DHSettingField;

typedef struct {
    DHSettingsEntryKind kind;
    DHSettingField field;
    char text[512];
} DHSettingsEntry;

int dh_settings_layout(DHSettingsEntry* out, int capacity);

int dh_settings_area_bar_width(void);

DHUiSettings dh_settings_load(void);
void dh_settings_save(uint32_t fps, uint32_t bitrate_mbps, uint32_t max_dim, uint32_t port,
    bool allow_input, bool client_control);

int dh_device_rows(DHDeviceRow* out, int capacity);
bool dh_same_device_addr(const char* a, const char* b);
uint16_t dh_default_port(void);

bool dh_client_control(void);
void dh_set_client_control(bool on);

int dh_device_name(char* out, int capacity);
void dh_set_device_name(const char* name);

int dh_bind_ip(char* out, int capacity);
void dh_set_bind_ip(const char* ip);

bool dh_autostart_enabled(void);
void dh_set_autostart(bool on);

bool dh_auto_share(void);
void dh_set_auto_share(bool on);

bool dh_clipboard_sync(void);
void dh_set_clipboard_sync(bool on);
void dh_set_transfer_dir(const char* dir);

bool dh_share_audio(void);
void dh_set_share_audio(bool on);

bool dh_play_audio(void);
void dh_set_play_audio(bool on);

bool dh_start_hidden(void);
void dh_set_start_hidden(bool on);

bool dh_keep_awake(void);
void dh_set_keep_awake(bool on);

int dh_version_line(char* out, int capacity);
const char* dh_local_addresses(void);
int dh_idle_host_status(uint16_t port, char* out, int capacity);
int dh_sharing_status(uint16_t port, bool allow_input, bool screen, bool terminal, bool files,
    char* out, int capacity);

typedef struct {
    char name[80];
    char shortKey[16];
    char fingerprint[64];
    int64_t pairedUnix;
    int64_t lastSeenUnix;
} DHPairedDevice;

int dh_paired_devices(DHPairedDevice* out, int capacity);
bool dh_paired_add_public_key(const char* public_key);
bool dh_paired_forget(const char* fingerprint);
void dh_paired_forget_all(void);
int dh_host_fingerprint(char* out, int capacity);

#ifdef __cplusplus
}
#endif
