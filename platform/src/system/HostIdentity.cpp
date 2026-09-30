#include "deskhubp/system/HostIdentity.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <openssl/nid.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AppDataFile.h"

namespace deskhubp {

namespace {

constexpr long kTransportCertLifetimeSeconds = 20L * 365 * 24 * 3600;
constexpr int kSerialBits = 63;
constexpr const char* kTransportCertCommonName = "deskhub";

struct BioDeleter {
    void operator()(BIO* bio) const {
        BIO_free(bio);
    }
};
struct X509Deleter {
    void operator()(X509* cert) const {
        X509_free(cert);
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
struct BignumDeleter {
    void operator()(BIGNUM* bn) const {
        BN_free(bn);
    }
};

using BioPtr = std::unique_ptr<BIO, BioDeleter>;
using X509Ptr = std::unique_ptr<X509, X509Deleter>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, PkeyDeleter>;
using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, PkeyCtxDeleter>;
using BignumPtr = std::unique_ptr<BIGNUM, BignumDeleter>;

std::string BioToString(BIO* bio) {
    const uint8_t* data = nullptr;
    size_t len = 0;
    if (BIO_mem_contents(bio, &data, &len) != 1) return {};
    return std::string(reinterpret_cast<const char*>(data), len);
}

bool UsableForTls(EVP_PKEY* key) {
    if (key == nullptr || EVP_PKEY_id(key) != EVP_PKEY_EC) return false;
    const EC_KEY* ec = EVP_PKEY_get0_EC_KEY(key);
    if (ec == nullptr) return false;
    const EC_GROUP* group = EC_KEY_get0_group(ec);
    return group != nullptr && EC_GROUP_get_curve_name(group) == NID_X9_62_prime256v1;
}

std::vector<uint8_t> SpkiOf(EVP_PKEY* key) {
    uint8_t* der = nullptr;
    const int len = i2d_PUBKEY(key, &der);
    if (len <= 0 || der == nullptr) return {};
    std::vector<uint8_t> out(der, der + len);
    OPENSSL_free(der);
    return out;
}

std::optional<deskhub::Fingerprint> FingerprintOfSpki(std::span<const uint8_t> spki) {
    if (spki.empty()) return std::nullopt;
    deskhub::Fingerprint fp;
    SHA256(spki.data(), spki.size(), fp.bytes.data());
    return fp;
}

std::optional<deskhub::Fingerprint> FingerprintOfCert(X509* cert) {
    X509_PUBKEY* pubkey = X509_get_X509_PUBKEY(cert);
    if (pubkey == nullptr) return std::nullopt;
    uint8_t* der = nullptr;
    const int len = i2d_X509_PUBKEY(pubkey, &der);
    if (len <= 0 || der == nullptr) return std::nullopt;
    const auto fp = FingerprintOfSpki(std::span<const uint8_t>(der, size_t(len)));
    OPENSSL_free(der);
    return fp;
}

PkeyPtr GenerateKey() {
    PkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr));
    if (!ctx) return nullptr;
    if (EVP_PKEY_keygen_init(ctx.get()) != 1) return nullptr;
    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_X9_62_prime256v1) != 1)
        return nullptr;
    EVP_PKEY* raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw) != 1) return nullptr;
    return PkeyPtr(raw);
}

PkeyPtr PrivateKeyFromPem(std::string_view pem) {
    if (pem.empty()) return nullptr;
    BioPtr bio(BIO_new_mem_buf(pem.data(), int(pem.size())));
    if (!bio) return nullptr;
    return PkeyPtr(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
}

bool SetRandomSerial(X509* cert) {
    BignumPtr serial(BN_new());
    if (!serial) return false;
    if (BN_rand(serial.get(), kSerialBits, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY) != 1) return false;
    return BN_to_ASN1_INTEGER(serial.get(), X509_get_serialNumber(cert)) != nullptr;
}

bool SetSubject(X509* cert) {
    X509_NAME* name = X509_get_subject_name(cert);
    if (name == nullptr) return false;
    const std::string cn(kTransportCertCommonName);
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
            reinterpret_cast<const uint8_t*>(cn.data()), int(cn.size()), -1, 0) != 1)
        return false;
    return X509_set_issuer_name(cert, name) == 1;
}

X509Ptr MakeSelfSigned(EVP_PKEY* key) {
    X509Ptr cert(X509_new());
    if (!cert) return nullptr;
    if (X509_set_version(cert.get(), X509_VERSION_3) != 1) return nullptr;
    if (!SetRandomSerial(cert.get())) return nullptr;
    if (X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0) == nullptr) return nullptr;
    if (X509_gmtime_adj(X509_getm_notAfter(cert.get()), kTransportCertLifetimeSeconds) == nullptr)
        return nullptr;
    if (!SetSubject(cert.get())) return nullptr;
    if (X509_set_pubkey(cert.get(), key) != 1) return nullptr;
    if (X509_sign(cert.get(), key, EVP_sha256()) == 0) return nullptr;
    return cert;
}

HostIdentity IdentityFromPem(std::string keyPem) {
    HostIdentity out;
    if (keyPem.empty()) return out;
    const PkeyPtr key = PrivateKeyFromPem(keyPem);
    if (!key) {
        LOGE("host identity: the stored private key cannot be read");
        return out;
    }
    if (!UsableForTls(key.get())) {
        LOGE("host identity: the stored private key is not an ECDSA P-256 key, so TLS cannot use it");
        return out;
    }
    out.publicKey = SpkiOf(key.get());
    const std::optional<deskhub::Fingerprint> fp = FingerprintOfSpki(out.publicKey);
    if (!fp) return {};
    out.keyPem = std::move(keyPem);
    out.fingerprint = *fp;
    out.keyPath = AppDataFilePath(kHostKeyFileName).string();
    return out;
}

bool HasStoredFile(const std::filesystem::path& path, bool& present) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory) return false;
    present = status.type() != std::filesystem::file_type::not_found;
    return true;
}

}

bool QuicAvailable() {
    return true;
}

std::optional<deskhub::Fingerprint> FingerprintOfCertDer(std::span<const uint8_t> der) {
    if (der.empty()) return std::nullopt;
    const uint8_t* at = der.data();
    X509Ptr cert(d2i_X509(nullptr, &at, long(der.size())));
    if (!cert) return std::nullopt;
    return FingerprintOfCert(cert.get());
}

HostIdentity LoadHostIdentity() {
    return IdentityFromPem(ReadAppDataFile(kHostKeyFileName));
}

HostIdentity LoadOrCreateHostIdentity() {
    HostIdentity existing = LoadHostIdentity();
    if (existing.Valid()) return existing;
    const auto keyPath = AppDataFilePath(kHostKeyFileName);
    if (keyPath.empty()) return {};
    bool hasKey = false;
    if (!HasStoredFile(keyPath, hasKey)) return {};
    if (hasKey) {
        LOGE("host identity: the stored private key is unusable; refusing to replace it");
        return {};
    }

    PkeyPtr key = GenerateKey();
    if (!key) {
        LOGE("host identity: could not generate a key pair");
        return {};
    }
    BioPtr keyBio(BIO_new(BIO_s_mem()));
    if (!keyBio) return {};
    if (PEM_write_bio_PKCS8PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr,
            nullptr) != 1)
        return {};
    const std::string keyPem = BioToString(keyBio.get());
    if (!WriteAppDataFileAtomic(kHostKeyFileName, keyPem)) {
        LOGE("host identity: could not save the key to the app data directory");
        return {};
    }

    HostIdentity created = IdentityFromPem(keyPem);
    if (created.Valid())
        LOGI("host identity: created %s", deskhub::FormatFingerprint(created.fingerprint).c_str());
    return created;
}

std::string TransportCertificatePem(const HostIdentity& identity) {
    const PkeyPtr key = PrivateKeyFromPem(identity.keyPem);
    if (!key || !UsableForTls(key.get())) return {};
    const X509Ptr cert = MakeSelfSigned(key.get());
    if (!cert) {
        LOGE("host identity: could not wrap the key in a certificate for TLS");
        return {};
    }
    BioPtr certBio(BIO_new(BIO_s_mem()));
    if (!certBio || PEM_write_bio_X509(certBio.get(), cert.get()) != 1) return {};
    return BioToString(certBio.get());
}

}
