#include "deskhub/ui/ClientKeys.h"

#include "deskhub/ui/HostProfiles.h"
#include "deskhub/ui/UiSettings.h"

namespace deskhub::ui {

namespace {

bool IsNameStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

}

bool IsValidClientKeyName(std::string_view name) {
    if (name.empty() || name.size() > kMaxClientKeyNameBytes) return false;
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (IsNameStart(c)) continue;
        if (i == 0 || (c != '-' && c != '_')) return false;
    }
    return true;
}

std::string ClientKeyLabel(std::string_view deviceName, std::string_view keyName) {
    std::string device = TruncateDeviceName(deviceName);
    if (keyName.empty() || keyName == kDefaultIdentityName) return device;
    const std::string key(keyName);
    return TruncateDeviceName(device.empty() ? key : device + " (" + key + ")");
}

const char* ClientKeyErrorText(ClientKeyError error) {
    switch (error) {
        case ClientKeyError::None: return "";
        case ClientKeyError::InvalidName:
            return "A key name is 1-64 ASCII letters, digits, '-' or '_', starting with a letter "
                   "or digit.";
        case ClientKeyError::NameInUse: return "A key with that name already exists.";
        case ClientKeyError::UnreadableKey:
            return "That file is not an Ed25519 or ECDSA P-256 private key in OpenSSH or PKCS#8 "
                   "form, or the passphrase is wrong.";
        case ClientKeyError::WriteFailed: return "The key could not be saved on this machine.";
        case ClientKeyError::DefaultKey:
            return "The default key cannot be deleted; this device always keeps one.";
        case ClientKeyError::KeyInUse:
            return "A trusted host still connects with this key. Remove that host first.";
        case ClientKeyError::KeyMissing: return "There is no key with that name.";
    }
    return "";
}

}
