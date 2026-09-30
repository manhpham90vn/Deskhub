#include "deskhubp/ffi/PairingFfi.h"

#include "deskhub/auth/PairingTokens.h"

#include <string>

#include "deskhub/net/PairingInvite.h"
#include "deskhub/qr/QrCode.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/host/PairingInvite.h"
#include "deskhubp/system/PairingTokenFile.h"
#include "deskhubp/system/UiSettingsStore.h"

extern "C" {

int dh_pairing_invite(uint16_t port, const char* bind_ip, char* out, int capacity) {
    const std::string invite = deskhubp::BuildPairingInvite(port, bind_ip ? bind_ip : "",
        deskhub::ui::TruncateDeviceName(deskhubp::SessionDeviceName()));
    return deskhubp::FillText(out, capacity, invite);
}

void dh_pairing_revoke(void) {
    deskhubp::RevokePairingTokens();
}

int64_t dh_pairing_token_ttl_seconds(void) {
    return deskhub::kPairingTokenTtlSeconds;
}

int dh_qr_encode(const char* text, uint8_t* modules, int capacity) {
    if (!text || !modules || capacity <= 0) return 0;
    const auto code = deskhub::EncodeQr(text);
    if (!code || code->size <= 0 || size_t(code->size) * size_t(code->size) > size_t(capacity))
        return 0;
    for (int y = 0; y < code->size; ++y)
        for (int x = 0; x < code->size; ++x)
            modules[size_t(y) * size_t(code->size) + size_t(x)] = code->Dark(x, y) ? 1 : 0;
    return code->size;
}

int dh_pairing_invite_address(const char* invite, char* out, int capacity) {
    if (!invite) return deskhubp::FillText(out, capacity, std::string());
    const auto parsed = deskhub::ParsePairingInvite(invite);
    if (!parsed || parsed->endpoints.empty()) return deskhubp::FillText(out, capacity, std::string());
    return deskhubp::FillText(out, capacity, deskhub::FormatPairingEndpoint(parsed->endpoints.front()));
}
}
