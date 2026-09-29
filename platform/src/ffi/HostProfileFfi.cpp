#include "deskhubp/ffi/HostProfileFfi.h"

#include <string>

#include "deskhub/ui/HostProfiles.h"
#include "deskhubp/client/HostProfiles.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/system/TrustStoreFile.h"

namespace {

namespace ui = deskhub::ui;

static_assert(int(DHHostProfileWriteFailed) == int(ui::HostProfileError::WriteFailed),
    "DHHostProfileError must carry the core value of every ui::HostProfileError");

std::string TextOf(const char* text) {
    return text ? std::string(text) : std::string();
}

}

extern "C" {

int dh_host_profiles(DHHostProfile* out, int capacity) {
    const auto store = deskhubp::TryLoadTrustStore();
    if (!store) return DH_HOST_PROFILES_UNREADABLE;
    if (!out || capacity <= 0) return int(store->Size());
    int count = 0;
    for (const deskhub::TrustedHost& host : store->Hosts()) {
        if (count == capacity) break;
        DHHostProfile& row = out[count++];
        deskhubp::CopyToBuf(row.alias, sizeof(row.alias), host.label);
        deskhubp::CopyToBuf(row.endpoint, sizeof(row.endpoint), host.endpoint);
        deskhubp::CopyToBuf(row.identity, sizeof(row.identity), ui::ProfileIdentityName(host));
        deskhubp::CopyToBuf(row.fingerprint, sizeof(row.fingerprint),
            deskhub::FormatFingerprint(host.fingerprint));
    }
    return count;
}

DHHostProfileError dh_host_profile_remove(const char* alias) {
    return DHHostProfileError(deskhubp::RemoveHostProfile(TextOf(alias)));
}

const char* dh_host_profile_error_text(DHHostProfileError error) {
    return ui::HostProfileErrorText(ui::HostProfileError(error));
}

DHHostProfileError dh_host_trust_new(const char* address, const char* fingerprint) {
    const auto key = deskhub::ParseFingerprint(TextOf(fingerprint));
    if (!key) return DHHostProfileInvalidHostKey;
    return DHHostProfileError(deskhubp::TrustNewHost(TextOf(address), *key));
}

int dh_trust_new_host_prompt(const char* address, const char* fingerprint, char* out,
    int capacity) {
    return deskhubp::FillText(out, capacity,
        ui::TrustNewHostPrompt(TextOf(address), TextOf(fingerprint)));
}
}
