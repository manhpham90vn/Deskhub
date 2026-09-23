#include "deskhubp/system/HostIdentity.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <openssl/nid.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include <memory>
#include <vector>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AppDataFile.h"

namespace deskhubp {

namespace {

constexpr long kIdentityLifetimeSeconds = 10L * 365 * 24 * 3600;
constexpr int kSerialBits = 63;

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

bool UsableForTls(X509* cert) {
    PkeyPtr key(X509_get_pubkey(cert));
    if (!key || EVP_PKEY_id(key.get()) != EVP_PKEY_EC) return false;
    const EC_KEY* ec = EVP_PKEY_get0_EC_KEY(key.get());
    if (ec == nullptr) return false;
    const EC_GROUP* group = EC_KEY_get0_group(ec);
    return group != nullptr && EC_GROUP_get_curve_name(group) == NID_X9_62_prime256v1;
}

std::optional<deskhub::Fingerprint> FingerprintOfCert(X509* cert) {
    X509_PUBKEY* pubkey = X509_get_X509_PUBKEY(cert);
    if (pubkey == nullptr) return std::nullopt;
    uint8_t* der = nullptr;
    const int len = i2d_X509_PUBKEY(pubkey, &der);
    if (len <= 0 || der == nullptr) return std::nullopt;
    deskhub::Fingerprint fp;
    SHA256(der, size_t(len), fp.bytes.data());
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

bool SetRandomSerial(X509* cert) {
    BignumPtr serial(BN_new());
    if (!serial) return false;
    if (BN_rand(serial.get(), kSerialBits, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY) != 1) return false;
    return BN_to_ASN1_INTEGER(serial.get(), X509_get_serialNumber(cert)) != nullptr;
}

bool SetSubject(X509* cert, std::string_view commonName) {
    X509_NAME* name = X509_get_subject_name(cert);
    if (name == nullptr) return false;
    const std::string cn(commonName.empty() ? std::string("deskhub-host") : std::string(commonName));
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
            reinterpret_cast<const uint8_t*>(cn.data()), int(cn.size()), -1, 0) != 1)
        return false;
    return X509_set_issuer_name(cert, name) == 1;
}

X509Ptr MakeSelfSigned(EVP_PKEY* key, std::string_view commonName) {
    X509Ptr cert(X509_new());
    if (!cert) return nullptr;
    if (X509_set_version(cert.get(), X509_VERSION_3) != 1) return nullptr;
    if (!SetRandomSerial(cert.get())) return nullptr;
    if (X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0) == nullptr) return nullptr;
    if (X509_gmtime_adj(X509_getm_notAfter(cert.get()), kIdentityLifetimeSeconds) == nullptr)
        return nullptr;
    if (!SetSubject(cert.get(), commonName)) return nullptr;
    if (X509_set_pubkey(cert.get(), key) != 1) return nullptr;
    if (X509_sign(cert.get(), key, EVP_sha256()) == 0) return nullptr;
    return cert;
}

HostIdentity IdentityFromPem(std::string certPem, std::string keyPem) {
    HostIdentity out;
    if (certPem.empty() || keyPem.empty()) return out;

    BioPtr bio(BIO_new_mem_buf(certPem.data(), int(certPem.size())));
    if (!bio) return out;
    X509Ptr cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
    if (!cert) return out;
    if (!UsableForTls(cert.get())) {
        LOGW("host identity: the stored key cannot be used for TLS, replacing it");
        return out;
    }
    const std::optional<deskhub::Fingerprint> fp = FingerprintOfCert(cert.get());
    if (!fp) return out;
    out.certPem = std::move(certPem);
    out.keyPem = std::move(keyPem);
    out.fingerprint = *fp;
    out.certPath = AppDataFilePath(kHostCertFileName).string();
    out.keyPath = AppDataFilePath(kHostKeyFileName).string();
    return out;
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
    return IdentityFromPem(ReadAppDataFile(kHostCertFileName), ReadAppDataFile(kHostKeyFileName));
}

HostIdentity LoadOrCreateHostIdentity(std::string_view commonName) {
    HostIdentity existing = LoadHostIdentity();
    if (existing.Valid()) return existing;

    PkeyPtr key = GenerateKey();
    if (!key) {
        LOGE("host identity: could not generate a key pair");
        return {};
    }
    X509Ptr cert = MakeSelfSigned(key.get(), commonName);
    if (!cert) {
        LOGE("host identity: could not build a self-signed certificate");
        return {};
    }

    BioPtr certBio(BIO_new(BIO_s_mem()));
    BioPtr keyBio(BIO_new(BIO_s_mem()));
    if (!certBio || !keyBio) return {};
    if (PEM_write_bio_X509(certBio.get(), cert.get()) != 1) return {};
    if (PEM_write_bio_PKCS8PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr,
            nullptr) != 1)
        return {};

    const std::string certPem = BioToString(certBio.get());
    const std::string keyPem = BioToString(keyBio.get());
    if (!WriteAppDataFile(kHostCertFileName, certPem) ||
        !WriteAppDataFile(kHostKeyFileName, keyPem)) {
        LOGE("host identity: could not save the key pair to the app data directory");
        return {};
    }

    HostIdentity created = IdentityFromPem(certPem, keyPem);
    if (created.Valid())
        LOGI("host identity: created %s", deskhub::FormatFingerprint(created.fingerprint).c_str());
    return created;
}

}
