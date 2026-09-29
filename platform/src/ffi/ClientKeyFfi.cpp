#include "deskhubp/ffi/ClientKeyFfi.h"

#include <string>

#include "deskhub/net/TrustStore.h"
#include "deskhub/ui/ClientKeys.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/ClientKeys.h"

namespace {

static_assert(int(DHClientKeyWriteFailed) == int(deskhub::ui::ClientKeyError::WriteFailed),
    "DHClientKeyError must carry the core value of every ui::ClientKeyError");

std::string TextOf(const char* text) {
    return text ? std::string(text) : std::string();
}

}

extern "C" {

int dh_client_keys(DHClientKey* out, int capacity) {
    int count = 0;
    for (const deskhubp::ClientIdentityInfo& key : deskhubp::LoadClientKeys()) {
        if (out && count < capacity) {
            deskhubp::CopyToBuf(out[count].name, sizeof(out[count].name), key.name);
            deskhubp::CopyToBuf(out[count].fingerprint, sizeof(out[count].fingerprint),
                deskhub::FormatFingerprint(key.fingerprint));
        }
        ++count;
    }
    return out && count > capacity ? capacity : count;
}

int dh_client_public_key(const char* name, char* out, int capacity) {
    const deskhubp::ClientIdentity identity = deskhubp::LoadClientIdentity(TextOf(name));
    if (!identity.Valid()) return deskhubp::FillText(out, capacity, std::string());
    return deskhubp::FillText(out, capacity, deskhubp::ClientPublicKeyLine(identity, TextOf(name)));
}

DHClientKeyError dh_client_key_generate(const char* name) {
    return DHClientKeyError(deskhubp::CreateClientKey(TextOf(name)));
}

DHClientKeyError dh_client_key_import(const char* name, const char* private_key,
    const char* passphrase) {
    return DHClientKeyError(
        deskhubp::ImportClientKey(TextOf(name), TextOf(private_key), TextOf(passphrase)));
}

const char* dh_client_key_error_text(DHClientKeyError error) {
    return deskhub::ui::ClientKeyErrorText(deskhub::ui::ClientKeyError(error));
}
}
