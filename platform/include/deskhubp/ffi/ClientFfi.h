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
    DHStrPickerEachWindow = 5,
    DHStrSharingTitle = 7,
    DHStrSharingConnectHint = 9,
    DHStrNothingShared = 10,
    DHStrStopSharing = 11,
    DHStrQueryingSources = 12,
    DHStrViewerOpenFailed = 13,
    DHStrConnectionEndedTitle = 14,
    DHStrInvalidAddressHint = 17,
    DHStrSessionEnded = 18,
    DHStrShareStartFailed = 19,
    DHStrScreenRecordingRequired = 20,
    DHStrSidebarHost = 29,
    DHStrSidebarClient = 30,
    DHStrSidebarSettings = 31,
    DHStrHostHeading = 32,
    DHStrClientHeading = 33,
    DHStrProjectUrl = 36,
    DHStrProjectLinkLabel = 37,
    DHStrRequestControlLabel = 39,
    DHStrClientIpPlaceholder = 40,
    DHStrNoDisplayTicked = 43,
    DHStrShareClampWarning = 46,
    DHStrShareStateOn = 47,
    DHStrShareStateOff = 48,
    DHStrStartSharing = 49,
    DHStrStartingShare = 50,
    DHStrStopDisplayAction = 52,
    DHStrDisconnectViewerAction = 53,
    DHStrNotSharing = 55,
    DHStrClientSettingsHeading = 57,
    DHStrClientSettingsHint = 58,
    DHStrUdpPortLabel = 59,
    DHStrBroadcastMemoryLabel = 60,
    DHStrBindInterfaceLabel = 61,
    DHStrBindAllInterfaces = 62,
    DHStrClipboardSyncLabel = 65,
    DHStrTrayShowWindow = 67,
    DHStrTrayHideWindow = 68,
    DHStrTrayQuit = 69,
    DHStrBindNotConnectedNote = 70,
    DHStrSettingsSectionConnection = 72,
    DHStrSettingsSectionSession = 74,
    DHStrKeepAwakeLabel = 76,
    DHStrSidebarDevices = 80,
    DHStrPairedHeading = 81,
    DHStrPairedHint = 82,
    DHStrPairedEmpty = 83,
    DHStrPairedForget = 84,
    DHStrPairedForgetAll = 85,
    DHStrPairedForgetAllPrompt = 86,
    DHStrThisMachineHeading = 89,
    DHStrThisMachineHint = 90,
    DHStrPairedColumnName = 91,
    DHStrPairedColumnKey = 92,
    DHStrTerminalPickerLabel = 103,
    DHStrOpenDesktopLabel = 105,
    DHStrOpenShellLabel = 106,
    DHStrTerminalExtraKeysHint = 108,
    DHStrPickSourcesHint = 109,
    DHStrPairedForgetNote = 110,
    DHStrDevicesHeading = 113,
    DHStrDeviceNameLabel = 115,
    DHStrConnectButton = 116,
    DHStrCopyButton = 117,
    DHStrAttachShellAction = 122,
    DHStrTerminalLocalWindowTitle = 123,
    DHStrTerminalAttachedHere = 124,
    DHStrTerminalClosed = 125,
    DHStrMobileHostNote = 126,
    DHStrWaitingForDisplays = 128,
    DHStrNoDisplayFound = 129,
    DHStrShareAudioLabel = 130,
    DHStrPlayAudioLabel = 131,
    DHStrFilesPickerLabel = 133,
    DHStrTransferChooseButton = 135,
    DHStrTransferCancelButton = 136,
    DHStrTransferSending = 138,
    DHStrTransferSendHeading = 141,
    DHStrTransferNoneChosen = 142,
    DHStrTransferTooManyFiles = 144,
    DHStrOpenFilesLabel = 145,
    DHStrTransferSentHeading = 146,
    DHStrTransferArrivedTitle = 148,
    DHStrConnectedPickSession = 150,
    DHStrDisconnectButton = 152,
    DHStrLinkReattaching = 157,
    DHStrShellPickerTitle = 158,
    DHStrShellPickerEmpty = 159,
    DHStrShellPickerNew = 161,
    DHStrShellPickerClose = 162,
    DHStrShellPickerCloseAsk = 163,
    DHStrReceivingFilesState = 168,
    DHStrMobileTakesFilesNote = 169,
    DHStrOpenFolderAction = 170,
    DHStrSavedHostsHeading = 171,
    DHStrSavedHostsHint = 172,
    DHStrSavedHostsEmpty = 173,
    DHStrRemoveHostAction = 180,
    DHStrAllowClientPlaceholder = 185,
    DHStrAllowClientAction = 186,
    DHStrAllowClientInvalid = 187,
    DHStrCopyPublicKeyAction = 190,
    DHStrTrustNewHostTitle = 195,
    DHStrTrustNewHostAction = 196,
    DHStrCancelAction = 197,
    DHStrCopiedButton = 198,
    DHStrDeviceNameHint = 199,
    DHStrShowQrAction = 202,
    DHStrHideQrAction = 203,
    DHStrScanQrAction = 204,
    DHStrQrHint = 205,
    DHStrAccessRequestsHeading = 206,
    DHStrAccessRequestsEmpty = 207,
    DHStrApproveAction = 208,
    DHStrDenyAction = 209,
    DHStrCameraDenied = 210,
    DHStrInviteInvalid = 211,
    DHStrHostLastAddressLabel = 212,
    DHStrAccessRequestNotificationTitle = 213,
    DHStrUnnamedClient = 214,
    DHStrLanDevicesEmpty = 215,
    DHStrHostAddressLabel = 216,
    DHStrShareNoHostIdentity = 217,
    DHStrHostNameLabel = 218,
    DHStrHostKeyLabel = 219,
    DHStrQrWindowTitle = 220,
    DHStrQrExpiredNote = 221,
    DHStrNewQrAction = 222,
    DHStrQrUnavailable = 223,
} DHStringId;

typedef enum {
    DHSourceQueryOk = 0,
    DHSourceQueryUnreachable = 1,
    DHSourceQueryUntrustedHost = 2,
    DHSourceQueryRefused = 3,
    DHSourceQueryAwaitingApproval = 4,
    DHSourceQueryInviteMismatch = 5,
    DHSourceQueryLocalError = 6,
} DHSourceQueryFailure;

typedef enum {
    DHThemeSidebar = 0,
    DHThemeSidebarHover = 1,
    DHThemeNavText = 2,
    DHThemeFootnote = 3,
    DHThemeAccent = 4,
    DHThemeAccentPressed = 5,
    DHThemeAccentDisabled = 6,
    DHThemeHeading = 7,
    DHThemeMuted = 8,
    DHThemeDisabled = 9,
    DHThemeOnline = 10,
    DHThemeOffline = 11,
    DHThemeOfflinePressed = 12,
    DHThemeWarning = 13,
    DHThemeWarningPressed = 14,
    DHThemePage = 15,
    DHThemeBorder = 16,
    DHThemeControl = 17,
    DHThemeControlHover = 18,
    DHThemeControlPressed = 19,
    DHThemeRowLine = 20,
    DHThemeViewerRow = 21,
    DHThemePanelIdle = 22,
    DHThemePanelBusy = 23,
    DHThemePanelLive = 24,
    DHThemeInfoCard = 25,
} DHThemeColor;

const char* dh_string(DHStringId id);

uint32_t dh_theme_color(DHThemeColor color, bool dark);

bool dh_native_key_to_vk(int32_t native_key_code, int32_t* out_vk, int32_t* out_scan);

DHModifier dh_modifier_class(int32_t vk);

int32_t dh_vk_scancode(int32_t vk);

bool dh_is_lock_toggle_vk(int32_t vk);

bool dh_is_escape_vk(int32_t vk);

bool dh_parse_address(const char* address);

int dh_list_sources(const char* address, const char* pairing_invite, DHSourceInfo* out,
    int capacity, DHHostCaps* out_caps, char* failure, int failure_capacity, char* new_host_key,
    int new_host_key_capacity, char* answered_address, int answered_address_capacity,
    int* out_failure_kind);

void dh_list_sources_cancel(void);

int dh_source_query_status(char* out, int capacity);

bool dh_is_pairing_invite(const char* text);

int dh_access_request_notification(const char* name, const char* address, char* out, int capacity);

int dh_max_sources(void);

int dh_max_transfer_files(void);

uint32_t dh_auto_share_probe_ms(void);

DHAutoShareStep dh_auto_share_step(bool displays_ready, uint32_t waited_ms);

bool dh_connect_decision(const DHSourceInfo* sources, int count, uint8_t* out_source_id);

int dh_connecting_to(const char* address, char* out, int capacity);

int dh_could_not_connect(const char* address, char* out, int capacity);

int dh_source_query_failed(const char* address, char* out, int capacity);

int dh_udp_port_line(uint32_t port, char* out, int capacity);
int dh_qr_expiry_line(int64_t seconds_left, char* out, int capacity);

typedef struct {
    int32_t buttonMinWidth;
    int32_t buttonHeight;
    int32_t primaryButtonHeight;
    int32_t fieldHeight;
    int32_t addressFieldWidth;
    int32_t portFieldWidth;
    int32_t numberFieldWidth;
} DHControlMetrics;

DHControlMetrics dh_control_metrics(void);

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

void dh_use_shared_container(const char* container);

int dh_config_dir(char* out, int capacity);

void dh_viewer_opened(void);

bool dh_viewer_closed(void);

#ifdef __cplusplus
}
#endif
