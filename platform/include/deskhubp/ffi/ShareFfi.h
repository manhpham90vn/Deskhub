#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "deskhubp/ffi/TerminalFfi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DHShellLive = 0,
    DHShellDetached = 1,
    DHShellLocal = 2,
} DHShellState;

typedef struct {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    char name[256];
} DHShareSource;

typedef struct {
    bool viewer;
    bool terminal;
    bool files;
    uint8_t sourceId;
    uint32_t termId;
    uint8_t shellState;
    bool online;
    char viewerAddr[192];
    char source[256];
    char size[32];
    char viewers[16];
    char client[192];
    char capture[16];
    char send[16];
    char mbps[16];
    char rtt[32];
} DHHostRow;

typedef struct {
    uint32_t fps;
    uint32_t bitrateMbps;
    uint32_t maxDim;
} DHShareDefaults;

typedef struct {
    char label[16];
    uint32_t maxDim;
} DHQualityPreset;

DHShareDefaults dh_share_default_options(void);

int dh_share_quality_presets(DHQualityPreset* out, int capacity);

int dh_share_list_sources(DHShareSource* out, int capacity);

typedef struct {
    uint64_t addrPacked;
    char shortKey[16];
    char name[80];
} DHPairingRequest;

bool dh_share_start(const DHShareSource* sources, int count, uint32_t fps, uint32_t bitrate_mbps,
    uint32_t max_dim, uint16_t port, bool allow_input, const char* passcode, bool terminal,
    bool files);

void dh_share_offer_audio(const int16_t* pcm, int samples);

bool dh_share_audio_running(void);

bool dh_share_terminal_active(void);

bool dh_share_files_active(void);

void dh_share_stop_files(void);

int dh_share_files_dir(char* out, int capacity);

bool dh_share_open_files_folder(void);

void dh_share_kick_shell(uint32_t term_id);

void dh_share_stop_terminal(void);

bool dh_share_attach_shell(uint32_t term_id);

bool dh_share_local_shell_alive(uint32_t term_id);

void dh_share_close_local_shell(uint32_t term_id);

bool dh_share_local_grid(uint32_t term_id, uint32_t scrollOffset, DHTermCell* cells,
    uint32_t cellCapacity, DHTermGrid* outGrid);

void dh_share_local_send_key(uint32_t term_id, int32_t key, uint32_t codepoint, bool shift, bool alt,
    bool ctrl);

void dh_share_local_send_text(uint32_t term_id, const char* utf8);

void dh_share_local_resize(uint32_t term_id, uint16_t cols, uint16_t rows);

int dh_share_take_pairing_requests(DHPairingRequest* out, int capacity);

void dh_share_answer_pairing(uint64_t addr_packed, bool allowed);

void dh_share_stop(void);

void dh_share_stop_source(uint8_t source_id);

void dh_share_kick_viewer(uint8_t source_id, const char* viewer_addr);

bool dh_share_running(void);

int dh_share_rows(DHHostRow* out, int capacity);

const char* dh_share_last_error(void);

const char* dh_share_local_addresses(void);

int dh_share_bind_warning(char* out, int capacity);

void dh_share_clip_offer(const char* text);
int dh_share_clip_take(char* out, int capacity);

#ifdef __cplusplus
}
#endif
