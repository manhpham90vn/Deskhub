#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DHTermSession DHTermSession;
typedef struct DHTermSessionInfo DHTermSessionInfo;

typedef enum {
    DHTermIdle = 0,
    DHTermConnecting = 1,
    DHTermDeciding = 2,
    DHTermOpening = 3,
    DHTermLive = 4,
    DHTermReattaching = 5,
    DHTermRefused = 6,
    DHTermFailed = 7,
    DHTermEnded = 8,
} DHTermState;

typedef enum {
    DHTermKeyChar = 0,
    DHTermKeyEnter = 1,
    DHTermKeyBackspace = 2,
    DHTermKeyTab = 3,
    DHTermKeyEscape = 4,
    DHTermKeyUp = 5,
    DHTermKeyDown = 6,
    DHTermKeyRight = 7,
    DHTermKeyLeft = 8,
    DHTermKeyHome = 9,
    DHTermKeyEnd = 10,
    DHTermKeyPageUp = 11,
    DHTermKeyPageDown = 12,
    DHTermKeyInsert = 13,
    DHTermKeyDelete = 14,
    DHTermKeyF1 = 15,
    DHTermKeyF2 = 16,
    DHTermKeyF3 = 17,
    DHTermKeyF4 = 18,
    DHTermKeyF5 = 19,
    DHTermKeyF6 = 20,
    DHTermKeyF7 = 21,
    DHTermKeyF8 = 22,
    DHTermKeyF9 = 23,
    DHTermKeyF10 = 24,
    DHTermKeyF11 = 25,
    DHTermKeyF12 = 26,
} DHTermKeyCode;

#define DH_TERM_ATTR_BOLD (1u << 0)
#define DH_TERM_ATTR_DIM (1u << 1)
#define DH_TERM_ATTR_ITALIC (1u << 2)
#define DH_TERM_ATTR_UNDERLINE (1u << 3)
#define DH_TERM_ATTR_BLINK (1u << 4)
#define DH_TERM_ATTR_REVERSE (1u << 5)
#define DH_TERM_ATTR_HIDDEN (1u << 6)
#define DH_TERM_ATTR_STRIKE (1u << 7)

typedef struct {
    uint32_t codepoint;
    uint8_t fgR, fgG, fgB;
    uint8_t bgR, bgG, bgB;
    uint8_t attrs;
} DHTermCell;

typedef struct {
    uint16_t rows;
    uint16_t cols;
    uint16_t cursorRow;
    uint16_t cursorCol;
    bool cursorVisible;
    uint32_t scrollbackRows;
    uint32_t scrollOffset;
    uint64_t revision;
} DHTermGrid;

typedef struct {
    void (*onState)(int32_t state, const char* message, void* user);
    void (*onRedraw)(void* user);
    void (*onTrustAsked)(int32_t verdict, const char* fingerprint, void* user);
    void (*onSessions)(const DHTermSessionInfo* infos, uint32_t count, void* user);
    void* user;
} DHTermCallbacks;

#define DH_TERM_NAME_LEN 65
#define DH_TERM_LINE_LEN 192

struct DHTermSessionInfo {
    uint32_t termId;
    int32_t state;
    uint16_t cols;
    uint16_t rows;
    uint64_t openedUs;
    char name[DH_TERM_NAME_LEN];
    char line[DH_TERM_LINE_LEN];
    bool resumable;
    bool closable;
};

DHTermSession* dh_term_open(const char* address, const char* passcode, uint16_t cols,
    uint16_t rows, const DHTermCallbacks* callbacks);

DHTermSession* dh_term_open_deferred(const char* address, const char* passcode, uint16_t cols,
    uint16_t rows, const DHTermCallbacks* callbacks);

DHTermSession* dh_term_open_resumed(const char* address, const char* passcode, uint16_t cols,
    uint16_t rows, uint32_t resumeId, const DHTermCallbacks* callbacks);

void dh_term_open_new(DHTermSession* s);

void dh_term_resume(DHTermSession* s, uint32_t termId);

void dh_term_close_session(DHTermSession* s, uint32_t termId);

void dh_term_request_sessions(DHTermSession* s);

bool dh_term_sessions_known(DHTermSession* s);

uint32_t dh_term_session_count(DHTermSession* s);

bool dh_term_session_info(DHTermSession* s, uint32_t index, DHTermSessionInfo* out);

void dh_term_stop(DHTermSession* s);

int32_t dh_term_state(DHTermSession* s);

int dh_term_message(DHTermSession* s, char* out, int capacity);

int dh_term_fingerprint(DHTermSession* s, char* out, int capacity);

int32_t dh_term_verdict(DHTermSession* s);

void dh_term_accept_key(DHTermSession* s);

void dh_term_reject_key(DHTermSession* s);

bool dh_term_grid(DHTermSession* s, uint32_t scrollOffset, DHTermCell* cells,
    uint32_t cellCapacity, DHTermGrid* outGrid);

void dh_term_send_key(DHTermSession* s, int32_t key, uint32_t codepoint, bool shift, bool alt,
    bool ctrl);

void dh_term_send_text(DHTermSession* s, const char* utf8);

void dh_term_paste(DHTermSession* s, const char* utf8);

void dh_term_resize(DHTermSession* s, uint16_t cols, uint16_t rows);

#ifdef __cplusplus
}
#endif
