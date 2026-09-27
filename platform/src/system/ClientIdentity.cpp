#include "deskhubp/system/ClientIdentity.h"

#include <openssl/bio.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <openssl/nid.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <array>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"

namespace deskhubp {

namespace {

struct BioDeleter {
    void operator()(BIO* bio) const {
        BIO_free(bio);
    }
};

struct PkeyDeleter {
    void operator()(EVP_PKEY* key) const {
        EVP_PKEY_free(key);
    }
};

struct PkeyCtxDeleter {
    void operator()(EVP_PKEY_CTX* ctx) const {
        EVP_PKEY_CTX_free(ctx);
    }
};

using BioPtr = std::unique_ptr<BIO, BioDeleter>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, PkeyDeleter>;
using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, PkeyCtxDeleter>;

int ReadPassphrase(char* output, int capacity, int, void* user) {
    const auto* secret = static_cast<const std::string*>(user);
    if (!secret || secret->empty() || secret->size() > size_t(capacity)) return 0;
    std::memcpy(output, secret->data(), secret->size());
    return int(secret->size());
}

PkeyPtr ParsePrivateKey(std::string_view pem, std::string_view passphrase) {
    if (pem.empty() || pem.size() > 65536 || passphrase.size() > 4096) return nullptr;
    BioPtr bio(BIO_new_mem_buf(pem.data(), int(pem.size())));
    if (!bio) return nullptr;
    std::string secret(passphrase);
    PkeyPtr key(PEM_read_bio_PrivateKey(bio.get(), nullptr, ReadPassphrase, &secret));
    if (!key) return nullptr;
    const int type = EVP_PKEY_id(key.get());
    if (type == EVP_PKEY_ED25519) return key;
    if (type != EVP_PKEY_EC) return nullptr;
    const EC_KEY* ec = EVP_PKEY_get0_EC_KEY(key.get());
    if (!ec || !EC_KEY_get0_group(ec) ||
        EC_GROUP_get_curve_name(EC_KEY_get0_group(ec)) != NID_X9_62_prime256v1)
        return nullptr;
    return key;
}

std::string SerializePrivateKey(EVP_PKEY* key) {
    BioPtr bio(BIO_new(BIO_s_mem()));
    if (!bio || PEM_write_bio_PKCS8PrivateKey(bio.get(), key, nullptr, nullptr, 0, nullptr,
                    nullptr) != 1)
        return {};
    const uint8_t* data = nullptr;
    size_t length = 0;
    if (BIO_mem_contents(bio.get(), &data, &length) != 1) return {};
    return std::string(reinterpret_cast<const char*>(data), length);
}

ClientIdentity IdentityFromKey(EVP_PKEY* key, std::string keyPem) {
    ClientIdentity identity;
    uint8_t* der = nullptr;
    const int length = i2d_PUBKEY(key, &der);
    if (length <= 0 || der == nullptr) return identity;
    identity.publicKey.assign(der, der + length);
    OPENSSL_free(der);
    const auto fingerprint = FingerprintOfPublicKey(identity.publicKey);
    if (!fingerprint) return {};
    identity.keyPem = std::move(keyPem);
    identity.fingerprint = *fingerprint;
    return identity;
}

ClientIdentity IdentityFromPem(std::string_view pem) {
    PkeyPtr key = ParsePrivateKey(pem, {});
    if (!key) return {};
    return IdentityFromKey(key.get(), std::string(pem));
}

std::filesystem::path TemporaryPath(const std::filesystem::path& target) {
    std::array<uint8_t, 8> random{};
    if (RAND_bytes(random.data(), random.size()) != 1) return {};
    constexpr std::string_view hex = "0123456789abcdef";
    std::string suffix = ".tmp-";
    for (uint8_t byte : random) {
        suffix += hex[byte >> 4];
        suffix += hex[byte & 15];
    }
    std::filesystem::path temporary = target;
    temporary += suffix;
    return temporary;
}

bool WriteTemporary(const std::filesystem::path& path, std::string_view data) {
#ifdef _WIN32
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool okay = true;
    size_t position = 0;
    while (position < data.size()) {
        DWORD written = 0;
        const DWORD amount = DWORD(std::min<size_t>(data.size() - position, 65536));
        if (!WriteFile(file, data.data() + position, amount, &written, nullptr) || written == 0) {
            okay = false;
            break;
        }
        position += written;
    }
    if (okay) okay = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    return okay;
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    bool okay = true;
    size_t position = 0;
    while (position < data.size()) {
        const ssize_t count = ::write(fd, data.data() + position, data.size() - position);
        if (count <= 0) {
            okay = false;
            break;
        }
        position += size_t(count);
    }
    if (okay) okay = ::fsync(fd) == 0;
    if (::close(fd) != 0) okay = false;
    return okay;
#endif
}

bool SavePrivateKey(std::string_view pem, bool replace) {
    const auto path = AppDataFilePath(kClientKeyFileName);
    if (path.empty()) return false;
    const auto temporary = TemporaryPath(path);
    if (temporary.empty() || !WriteTemporary(temporary, pem)) {
        std::error_code ec;
        std::filesystem::remove(temporary, ec);
        return false;
    }
#ifdef _WIN32
    const DWORD flags = (replace ? MOVEFILE_REPLACE_EXISTING : 0) | MOVEFILE_WRITE_THROUGH;
    const bool saved = MoveFileExW(temporary.c_str(), path.c_str(), flags) != 0;
#else
    const bool saved = replace ? ::rename(temporary.c_str(), path.c_str()) == 0
                               : ::link(temporary.c_str(), path.c_str()) == 0;
    if (!replace) ::unlink(temporary.c_str());
#endif
    if (!saved) {
        std::error_code ec;
        std::filesystem::remove(temporary, ec);
    }
    return saved;
}

}

ClientIdentity::ClientIdentity(const HostIdentity& legacy) {
    keyPem = legacy.keyPem;
    publicKey = IdentityPublicKey(legacy);
    const auto derived = FingerprintOfPublicKey(publicKey);
    if (derived) fingerprint = *derived;
}

ClientIdentity LoadClientIdentity() {
    return IdentityFromPem(ReadAppDataFile(kClientKeyFileName));
}

ClientIdentity LoadOrCreateClientIdentity() {
    const auto path = AppDataFilePath(kClientKeyFileName);
    if (path.empty()) return {};
    std::error_code ec;
    if (std::filesystem::exists(path, ec) || ec) return LoadClientIdentity();

    PkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr));
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1) return {};
    EVP_PKEY* raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw) != 1) return {};
    PkeyPtr key(raw);
    const std::string pem = SerializePrivateKey(key.get());
    if (pem.empty()) return {};
    if (!SavePrivateKey(pem, false)) return LoadClientIdentity();
    return IdentityFromKey(key.get(), pem);
}

bool ImportClientIdentity(std::string_view privateKeyPem, std::string_view passphrase) {
    PkeyPtr key = ParsePrivateKey(privateKeyPem, passphrase);
    if (!key) return false;
    const std::string pem = SerializePrivateKey(key.get());
    if (pem.empty()) return false;
    const ClientIdentity identity = IdentityFromKey(key.get(), pem);
    if (!identity.Valid()) return false;
    constexpr std::array<uint8_t, 25> probe = {'D', 'e', 's', 'k', 'h', 'u', 'b', ' ',
        'k', 'e', 'y', ' ', 'i', 'm', 'p', 'o', 'r', 't', ' ', 's', 'e', 'l', 'f', '-', 't'};
    const auto signature = SignWithClientIdentity(identity, probe);
    if (signature.empty() || !VerifySignature(identity.publicKey, probe, signature)) return false;
    return SavePrivateKey(pem, true);
}

std::string ClientPublicKeyText(const ClientIdentity& identity) {
    return PublicKeyTextFromSpki(identity.publicKey);
}

std::vector<uint8_t> SignWithClientIdentity(const ClientIdentity& identity,
    std::span<const uint8_t> data) {
    HostIdentity signingKey;
    signingKey.keyPem = identity.keyPem;
    return SignWithIdentity(signingKey, data);
}

}
