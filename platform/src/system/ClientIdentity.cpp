#include "deskhubp/system/ClientIdentity.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <quiche.h>

#include <array>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dpapi.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "deskhub/ui/ClientKeys.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ConfigFileLock.h"

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

std::mutex& IdentityMutex() {
    static std::mutex mutex;
    return mutex;
}

#ifdef _WIN32
std::string ProtectPrivateKey(std::string_view pem) {
    DATA_BLOB input{DWORD(pem.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(pem.data()))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Deskhub client identity", nullptr, nullptr,
            nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return {};
    std::string encrypted("DHK1", 4);
    encrypted.append(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return encrypted;
}

std::string UnprotectPrivateKey(std::string_view encrypted) {
    if (!encrypted.starts_with("DHK1") || encrypted.size() <= 4) return {};
    DATA_BLOB input{DWORD(encrypted.size() - 4),
        reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data() + 4))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
            CRYPTPROTECT_UI_FORBIDDEN, &output)) return {};
    std::string pem(reinterpret_cast<const char*>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return pem;
}
#endif

std::string ReadPrivateKey(std::string_view fileName) {
    const auto path = AppDataFilePath(std::string(fileName));
    if (path.empty()) return {};
#ifdef _WIN32
    const std::string data = ReadAppDataFile(std::string(fileName));
    if (data.size() > 65536) return {};
    if (data.starts_with("DHK1")) return UnprotectPrivateKey(data);
    return data.starts_with("-----BEGIN ") ? data : std::string();
#else
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return {};
    struct stat info{};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_uid != geteuid() || (info.st_mode & 077) != 0 ||
        info.st_size <= 0 || info.st_size > 65536) {
        ::close(fd);
        return {};
    }
    std::string data(size_t(info.st_size), '\0');
    size_t position = 0;
    while (position < data.size()) {
        const ssize_t count = ::read(fd, data.data() + position, data.size() - position);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        position += size_t(count);
    }
    ::close(fd);
    return position == data.size() ? data : std::string();
#endif
}

int ReadPassphrase(char* output, int capacity, int, void* user) {
    const auto* secret = static_cast<const std::string*>(user);
    if (!secret || secret->empty() || secret->size() > size_t(capacity)) return 0;
    std::copy(secret->begin(), secret->end(), output);
    return int(secret->size());
}

PkeyPtr ParsePrivateKey(std::string_view pem, std::string_view passphrase) {
    if (pem.empty() || pem.size() > 65536 || passphrase.size() > 4096) return nullptr;
    if (pem.starts_with("-----BEGIN OPENSSH PRIVATE KEY-----")) {
        quiche_openssh_private_key decoded{};
        const int status = quiche_parse_openssh_private_key(
            reinterpret_cast<const uint8_t*>(pem.data()), pem.size(),
            reinterpret_cast<const uint8_t*>(passphrase.data()), passphrase.size(), &decoded);
        if (status != 0) return nullptr;
        const auto build = [&]() -> PkeyPtr {
            if (decoded.kind == 1 && decoded.public_key_len == 32) {
                PkeyPtr key(EVP_PKEY_new_raw_private_key(
                    EVP_PKEY_ED25519, nullptr, decoded.private_key, sizeof(decoded.private_key)));
                std::array<uint8_t, 32> publicKey{};
                size_t publicLength = publicKey.size();
                if (!key || EVP_PKEY_get_raw_public_key(key.get(), publicKey.data(), &publicLength) != 1 ||
                    publicLength != decoded.public_key_len ||
                    CRYPTO_memcmp(publicKey.data(), decoded.public_key, publicLength) != 0)
                    return nullptr;
                return key;
            }
            if (decoded.kind != 2 || decoded.public_key_len != 65) return nullptr;
            std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)> ec(
                EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), EC_KEY_free);
            std::unique_ptr<BIGNUM, decltype(&BN_clear_free)> scalar(
                BN_bin2bn(decoded.private_key, sizeof(decoded.private_key), nullptr),
                BN_clear_free);
            if (!ec || !scalar || EC_KEY_set_private_key(ec.get(), scalar.get()) != 1)
                return nullptr;
            const EC_GROUP* group = EC_KEY_get0_group(ec.get());
            std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)> point(
                EC_POINT_new(group), EC_POINT_free);
            if (!point || EC_POINT_mul(group, point.get(), scalar.get(), nullptr, nullptr, nullptr) != 1 ||
                EC_KEY_set_public_key(ec.get(), point.get()) != 1 ||
                EC_KEY_check_key(ec.get()) != 1)
                return nullptr;
            std::array<uint8_t, 65> publicKey{};
            if (EC_POINT_point2oct(group, point.get(), POINT_CONVERSION_UNCOMPRESSED,
                    publicKey.data(), publicKey.size(), nullptr) != publicKey.size() ||
                CRYPTO_memcmp(publicKey.data(), decoded.public_key, publicKey.size()) != 0)
                return nullptr;
            PkeyPtr key(EVP_PKEY_new());
            if (!key || EVP_PKEY_set1_EC_KEY(key.get(), ec.get()) != 1) return nullptr;
            return key;
        };
        PkeyPtr key = build();
        OPENSSL_cleanse(&decoded, sizeof(decoded));
        return key;
    }
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
    const std::string protectedData = ProtectPrivateKey(data);
    if (protectedData.empty()) return false;
    data = protectedData;
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
        if (count < 0 && errno == EINTR) continue;
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

std::optional<std::string> KeyFileName(std::string_view name) {
    if (name == "default") return std::string(kClientKeyFileName);
    if (!deskhub::ui::IsValidClientKeyName(name)) return std::nullopt;
    return "client_key." + std::string(name) + ".pem";
}

bool SavePrivateKey(std::string_view fileName, std::string_view pem, bool replace) {
    const std::lock_guard<std::mutex> lock(IdentityMutex());
    const ConfigFileLock fileLock(fileName);
    if (!fileLock.Valid()) return false;
    const auto path = AppDataFilePath(std::string(fileName));
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
    return LoadClientIdentity("default");
}

ClientIdentity LoadClientIdentity(std::string_view name) {
    const auto fileName = KeyFileName(name);
    if (!fileName) return {};
    const std::string pem = ReadPrivateKey(*fileName);
    const ClientIdentity identity = IdentityFromPem(pem);
#ifdef _WIN32
    if (identity.Valid() && ReadAppDataFile(*fileName).starts_with("-----BEGIN ") &&
        !SavePrivateKey(*fileName, pem, true)) return {};
#endif
    return identity;
}

ClientIdentity LoadOrCreateClientIdentity() {
    const auto path = AppDataFilePath(kClientKeyFileName);
    if (path.empty()) return {};
    std::error_code ec;
    if (std::filesystem::exists(path, ec) || ec) return LoadClientIdentity();

    const ClientIdentity generated = GenerateClientIdentity("default");
    return generated.Valid() ? generated : LoadClientIdentity();
}

ClientIdentity GenerateClientIdentity(std::string_view name) {
    const auto fileName = KeyFileName(name);
    if (!fileName) return {};
    PkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr));
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1) return {};
    EVP_PKEY* raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw) != 1) return {};
    PkeyPtr key(raw);
    const std::string pem = SerializePrivateKey(key.get());
    if (pem.empty()) return {};
    if (!SavePrivateKey(*fileName, pem, false)) return {};
    return IdentityFromKey(key.get(), pem);
}

bool ImportClientIdentity(std::string_view privateKeyPem, std::string_view passphrase) {
    return ImportClientIdentity("default", privateKeyPem, passphrase);
}

bool ImportClientIdentity(std::string_view name, std::string_view privateKeyPem,
    std::string_view passphrase) {
    const auto fileName = KeyFileName(name);
    if (!fileName) return false;
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
    return SavePrivateKey(*fileName, pem, name == "default");
}

std::vector<ClientIdentityInfo> ListClientIdentities() {
    std::vector<ClientIdentityInfo> out;
    const auto defaultPath = AppDataFilePath(kClientKeyFileName);
    if (defaultPath.empty()) return out;
    std::error_code error;
    if (std::filesystem::exists(defaultPath, error)) {
        const ClientIdentity identity = LoadClientIdentity();
        out.push_back(ClientIdentityInfo{"default", ClientPublicKeyText(identity),
            identity.fingerprint, identity.Valid()});
    }
    if (error) return out;

    constexpr std::string_view prefix = "client_key.";
    constexpr std::string_view suffix = ".pem";
    std::filesystem::directory_iterator it(defaultPath.parent_path(), error);
    const std::filesystem::directory_iterator end;
    while (!error && it != end) {
        const std::string file = it->path().filename().string();
        if (file.size() > prefix.size() + suffix.size() && file.starts_with(prefix) &&
            file.ends_with(suffix)) {
            const std::string name = file.substr(prefix.size(), file.size() - prefix.size() - suffix.size());
            if (KeyFileName(name) && name != "default") {
                const ClientIdentity identity = LoadClientIdentity(name);
                out.push_back(ClientIdentityInfo{name, ClientPublicKeyText(identity),
                    identity.fingerprint, identity.Valid()});
            }
        }
        it.increment(error);
    }
    std::sort(out.begin(), out.end(), [](const ClientIdentityInfo& a, const ClientIdentityInfo& b) {
        return a.name < b.name;
    });
    return out;
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
