#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DH_SOURCE_QUERY_FAILED (-1)

typedef enum {
    DHAutoShareKeepWaiting = 0,
    DHAutoShareShareNow = 1,
    DHAutoShareGiveUpWaiting = 2,
} DHAutoShareStep;

typedef enum {
    DHPhaseIdle = 0,
    DHPhaseConnecting = 1,
    DHPhaseStreaming = 2,
    DHPhaseEnded = 3,
    DHPhaseDeciding = 4,
    DHPhaseReattaching = 5,
} DHPhase;

typedef enum {
    DHLinkQualityUnknown = 0,
    DHLinkQualityGood = 1,
    DHLinkQualityFair = 2,
    DHLinkQualityPoor = 3,
} DHLinkQuality;

typedef struct {
    bool haveRtt;
    uint32_t rttMs;
    uint8_t lossPct;
    DHLinkQuality quality;
} DHLinkHealth;

typedef struct {
    uint8_t sourceId;
    uint16_t width;
    uint16_t height;
    char name[256];
    char displayName[256];
    char sizeLabel[32];
    char pickerLabel[320];
} DHSourceInfo;

typedef struct {
    bool acceptsInput;
    bool terminal;
    bool audio;
    bool files;
} DHHostCaps;

typedef struct {
    bool openShell;
    bool openFiles;
    bool openDesktop;
    bool showPicker;
    uint8_t sourceId;
    int32_t problem;
} DHConnectPlan;

typedef struct {
    double x;
    double y;
    double width;
    double height;
} DHViewRect;

typedef struct {
    double zoom;
    double panX;
    double panY;
} DHViewTransform;

typedef struct {
    double x;
    double y;
} DHCursor;

typedef struct {
    char label[16];
    int32_t vk;
    int32_t scan;
    int32_t modVk;
    int32_t modScan;
} DHHotkey;

typedef enum {
    DHModifierNone = 0,
    DHModifierShift = 1,
    DHModifierControl = 2,
    DHModifierOption = 3,
    DHModifierCommand = 4,
    DHModifierCapsLock = 5,
} DHModifier;

typedef struct {
    bool locked;
} DHPointerLock;

typedef struct {
    bool lockChanged;
    bool releaseHeldInput;
} DHPointerLockEffect;

typedef enum {
    DHStrAppTitle = 0,
    DHStrHostIpIntro = 1,
    DHStrNoNetworkAddress = 2,
    DHStrClientIpPrompt = 3,
    DHStrPickerTitle = 4,
    DHStrPickerEachWindow = 5,
    DHStrShareButton = 6,
    DHStrSharingTitle = 7,
    DHStrSharingSourcesIntro = 8,
    DHStrSharingConnectHint = 9,
    DHStrNothingShared = 10,
    DHStrStopSharing = 11,
    DHStrQueryingSources = 12,
    DHStrViewerOpenFailed = 13,
    DHStrConnectionEndedTitle = 14,
    DHStrDisconnected = 15,
    DHStrUdpPortLine = 16,
    DHStrInvalidAddressHint = 17,
    DHStrSessionEnded = 18,
    DHStrShareStartFailed = 19,
    DHStrScreenRecordingRequired = 20,
    DHStrClientPasscodePrompt = 21,
    DHStrClientPasscodeHint = 22,
    DHStrPasscodeInvalid = 23,
    DHStrPasscodeLabel = 24,
    DHStrLanDevicesHeading = 25,
    DHStrRecentDevicesHeading = 26,
    DHStrRecentDevicesHint = 27,
    DHStrRecentDevicesEmpty = 28,
    DHStrSidebarHost = 29,
    DHStrSidebarClient = 30,
    DHStrSidebarSettings = 31,
    DHStrHostHeading = 32,
    DHStrClientHeading = 33,
    DHStrSettingsHeading = 34,
    DHStrSettingsHint = 35,
    DHStrProjectUrl = 36,
    DHStrProjectLinkLabel = 37,
    DHStrAllowControlLabel = 38,
    DHStrRequestControlLabel = 39,
    DHStrClientIpPlaceholder = 40,
    DHStrConnectPromptTitle = 41,
    DHStrPickDisplaysHint = 42,
    DHStrNoDisplayTicked = 43,
    DHStrStopSelectedDisplay = 44,
    DHStrDisconnectSelectedViewer = 45,
    DHStrShareClampWarning = 46,
    DHStrShareStateOn = 47,
    DHStrShareStateOff = 48,
    DHStrStartSharing = 49,
    DHStrStartingShare = 50,
    DHStrRefreshNow = 51,
    DHStrStopDisplayAction = 52,
    DHStrDisconnectViewerAction = 53,
    DHStrLanDevicesHint = 54,
    DHStrNotSharing = 55,
    DHStrLanDevicesEmpty = 56,
    DHStrClientSettingsHeading = 57,
    DHStrClientSettingsHint = 58,
    DHStrUdpPortLabel = 59,
    DHStrBroadcastMemoryLabel = 60,
    DHStrBindInterfaceLabel = 61,
    DHStrBindAllInterfaces = 62,
    DHStrAutostartLabel = 63,
    DHStrAutoShareLabel = 64,
    DHStrClipboardSyncLabel = 65,
    DHStrCloseToTrayLabel = 66,
    DHStrTrayShowWindow = 67,
    DHStrTrayHideWindow = 68,
    DHStrTrayQuit = 69,
    DHStrBindNotConnectedNote = 70,
    DHStrSettingsSectionVideo = 71,
    DHStrSettingsSectionConnection = 72,
    DHStrSettingsSectionSecurity = 73,
    DHStrSettingsSectionSession = 74,
    DHStrSettingsSectionLaunch = 75,
    DHStrKeepAwakeLabel = 76,
    DHStrPairingRequestTitle = 77,
    DHStrPairingAllow = 78,
    DHStrPairingDeny = 79,
    DHStrSidebarDevices = 80,
    DHStrPairedHeading = 81,
    DHStrPairedHint = 82,
    DHStrPairedEmpty = 83,
    DHStrPairedForget = 84,
    DHStrPairedForgetAll = 85,
    DHStrPairedForgetAllPrompt = 86,
    DHStrAllowPairingLabel = 87,
    DHStrAllowPairingHint = 88,
    DHStrThisMachineHeading = 89,
    DHStrThisMachineHint = 90,
    DHStrPairedColumnName = 91,
    DHStrPairedColumnKey = 92,
    DHStrPairedColumnPaired = 93,
    DHStrPairedColumnLastSeen = 94,
    DHStrTrustNewHostTitle = 95,
    DHStrTrustNewHostBody = 96,
    DHStrTrustChangedTitle = 97,
    DHStrTrustChangedBody = 98,
    DHStrTrustFingerprintLabel = 99,
    DHStrTrustAccept = 100,
    DHStrTrustReject = 101,
    DHStrTerminalSourceName = 102,
    DHStrTerminalPickerLabel = 103,
    DHStrOpenChoiceGroup = 104,
    DHStrOpenDesktopLabel = 105,
    DHStrOpenShellLabel = 106,
    DHStrOpenNothingTicked = 107,
    DHStrTerminalExtraKeysHint = 108,
    DHStrPickSourcesHint = 109,
    DHStrPairedForgetNote = 110,
    DHStrOpenChoiceHint = 111,
    DHStrPasscodeHint = 112,
    DHStrDevicesHeading = 113,
    DHStrDeviceColumnWhere = 114,
    DHStrDeviceNameLabel = 115,
    DHStrConnectButton = 116,
    DHStrCopyButton = 117,
    DHStrFpsLabel = 118,
    DHStrBitrateLabel = 119,
    DHStrQualityLabel = 120,
    DHStrAttachShellAction = 122,
    DHStrTerminalLocalWindowTitle = 123,
    DHStrTerminalAttachedHere = 124,
    DHStrTerminalClosed = 125,
    DHStrMobileHostNote = 126,
    DHStrHostHasNoTerminal = 127,
    DHStrWaitingForDisplays = 128,
    DHStrNoDisplayFound = 129,
    DHStrShareAudioLabel = 130,
    DHStrPlayAudioLabel = 131,
    DHStrFilesSourceName = 132,
    DHStrFilesPickerLabel = 133,
    DHStrTransferHeading = 134,
    DHStrTransferChooseButton = 135,
    DHStrTransferCancelButton = 136,
    DHStrTransferFolderLabel = 137,
    DHStrTransferSending = 138,
    DHStrTransferDone = 139,
    DHStrTransferHostNotTaking = 140,
    DHStrTransferSendHeading = 141,
    DHStrTransferNoneChosen = 142,
    DHStrTransferBusyNote = 143,
    DHStrTransferTooManyFiles = 144,
    DHStrOpenFilesLabel = 145,
    DHStrTransferSentHeading = 146,
    DHStrTransferAcceptLabel = 147,
    DHStrTransferArrivedTitle = 148,
    DHStrTransferStopTakingButton = 149,
    DHStrConnectedPickSession = 150,
    DHStrConnectFirstHint = 151,
    DHStrDisconnectButton = 152,
    DHStrLinkQualityGood = 153,
    DHStrLinkQualityFair = 154,
    DHStrLinkQualityPoor = 155,
    DHStrLinkNoReading = 156,
    DHStrLinkReattaching = 157,
} DHStringId;

int dh_link_quality_text(DHLinkQuality quality, char* out, int capacity);

int dh_link_ping_text(bool haveRtt, uint32_t rttMs, char* out, int capacity);

const char* dh_string(DHStringId id);

int dh_pairing_request_body(const char* name, const char* address, const char* shortKey,
    char* out, int capacity);

bool dh_native_key_to_vk(int32_t native_key_code, int32_t* out_vk, int32_t* out_scan);

DHModifier dh_modifier_class(int32_t vk);

int32_t dh_vk_scancode(int32_t vk);

bool dh_is_lock_toggle_vk(int32_t vk);

bool dh_is_escape_vk(int32_t vk);

bool dh_parse_address(const char* address);

int dh_list_sources(const char* address, DHSourceInfo* out, int capacity, const char* passcode,
    DHHostCaps* out_caps);

bool dh_host_has_terminal(const char* address, const char* passcode);

bool dh_host_takes_files(const char* address, const char* passcode);

bool dh_is_valid_passcode(const char* passcode);

int dh_passcode_digits(void);

int dh_max_sources(void);

int dh_max_transfer_files(void);

uint32_t dh_auto_share_probe_ms(void);

DHAutoShareStep dh_auto_share_step(bool displays_ready, uint32_t waited_ms);

bool dh_connect_decision(const DHSourceInfo* sources, int count, uint8_t* out_source_id);

DHConnectPlan dh_connect_plan(DHHostCaps caps, const DHSourceInfo* sources, int count,
    bool want_desktop, bool want_shell, bool want_files);

int dh_connect_problem_text(int32_t problem, const char* address, char* out, int capacity);

int dh_connecting_to(const char* address, char* out, int capacity);

int dh_could_not_connect(const char* address, char* out, int capacity);

int dh_source_query_failed(const char* address, char* out, int capacity);

int dh_source_query_empty(const char* address, char* out, int capacity);

int dh_udp_port_line(uint32_t port, char* out, int capacity);

int dh_compose_address(const char* host, const char* portText, char* out, int capacity);

int dh_address_host(const char* address, char* out, int capacity);

uint32_t dh_address_port(const char* address);

int dh_host_title(const char* address, uint32_t width, uint32_t height, char* out, int capacity);

int dh_zoom_label(double zoom, char* out, int capacity);

bool dh_is_zoomed(double zoom);

bool dh_should_refit_viewer(uint32_t fitted_w, uint32_t fitted_h, uint32_t new_w,
    uint32_t new_h);

void dh_fit_viewer_window(uint32_t video_w, uint32_t video_h, uint32_t work_w, uint32_t work_h,
    uint32_t* out_w, uint32_t* out_h);

int dh_viewer_base_title(const char* sourceName, char* out, int capacity);

int dh_hotkeys(DHHotkey* out, int capacity);

DHViewRect dh_video_rect(double viewportW, double viewportH, double aspect, DHViewTransform t);

DHViewTransform dh_apply_gesture(DHViewTransform cur, double factor, double centroidX,
    double centroidY, double panDeltaX, double panDeltaY, double viewportW, double viewportH,
    double aspect);

bool dh_normalize_pointer(double px, double py, DHViewRect rect, int32_t* nx, int32_t* ny);

int32_t dh_take_scroll_notches(double dragPoints, double* carry);

int32_t dh_scroll_notches_from_lines(double lines);

DHCursor dh_cursor_clamp(DHCursor cur, DHViewRect video, double viewportW, double viewportH);

DHCursor dh_cursor_move(DHCursor cur, double dx, double dy, DHViewRect video, double viewportW,
    double viewportH);

bool dh_cursor_point(DHCursor cur, DHViewRect video, double* px, double* py);

bool dh_cursor_normalize(DHCursor cur, DHViewRect video, int32_t* nx, int32_t* ny);

DHPointerLockEffect dh_pointer_toggle_lock(DHPointerLock* state);

DHPointerLockEffect dh_pointer_escape(DHPointerLock* state);

DHPointerLockEffect dh_pointer_focus_lost(DHPointerLock* state);

int dh_pointer_subtitle(DHPointerLock state, const char* statusLine, char* out, int capacity);

int dh_view_only_subtitle(const char* statusLine, char* out, int capacity);

int dh_invalid_address_line(const char* address, char* out, int capacity);

void dh_set_data_dir(const char* dir);

void dh_viewer_opened(void);

bool dh_viewer_closed(void);

#ifdef __cplusplus
}
#endif
