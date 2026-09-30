#include "deskhubp/system/AuthProof.h"

#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/nid.h>

#include "deskhubp/diag/Log.h"

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

struct MdCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const {
        EVP_MD_CTX_free(ctx);
    }
};

struct EcKeyDeleter {
    void operator()(EC_KEY* key) const {
        EC_KEY_free(key);
    }
};

struct EcPointDeleter {
    void operator()(EC_POINT* point) const {
        EC_POINT_free(point);
    }
};

using BioPtr = std::unique_ptr<BIO, BioDeleter>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, PkeyDeleter>;
using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, MdCtxDeleter>;
using EcKeyPtr = std::unique_ptr<EC_KEY, EcKeyDeleter>;
using EcPointPtr = std::unique_ptr<EC_POINT, EcPointDeleter>;

void AppendSshString(std::vector<uint8_t>& out, std::span<const uint8_t> value) {
    const uint32_t size = uint32_t(value.size());
    out.push_back(uint8_t(size >> 24));
    out.push_back(uint8_t(size >> 16));
    out.push_back(uint8_t(size >> 8));
    out.push_back(uint8_t(size));
    out.insert(out.end(), value.begin(), value.end());
}

void AppendSshString(std::vector<uint8_t>& out, std::string_view value) {
    AppendSshString(out,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data()), value.size()));
}

std::span<const uint8_t> ReadSshString(std::span<const uint8_t>& input) {
    if (input.size() < 4) return {};
    const size_t size = (size_t(input[0]) << 24) | (size_t(input[1]) << 16) |
                        (size_t(input[2]) << 8) | size_t(input[3]);
    input = input.subspan(4);
    if (size > input.size()) return {};
    const auto value = input.first(size);
    input = input.subspan(size);
    return value;
}

std::vector<uint8_t> SpkiFromKey(EVP_PKEY* key) {
    uint8_t* der = nullptr;
    const int length = i2d_PUBKEY(key, &der);
    if (length <= 0 || der == nullptr) return {};
    std::vector<uint8_t> out(der, der + length);
    OPENSSL_free(der);
    return out;
}

PkeyPtr PrivateKeyFromPem(std::string_view pem) {
    if (pem.empty()) return nullptr;
    BioPtr bio(BIO_new_mem_buf(pem.data(), int(pem.size())));
    if (!bio) return nullptr;
    return PkeyPtr(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
}

PkeyPtr PublicKeyFromSpki(std::span<const uint8_t> spkiDer) {
    if (spkiDer.empty()) return nullptr;
    const uint8_t* at = spkiDer.data();
    X509_PUBKEY* pubkey = d2i_X509_PUBKEY(nullptr, &at, long(spkiDer.size()));
    if (pubkey == nullptr) return nullptr;
    PkeyPtr key(X509_PUBKEY_get(pubkey));
    X509_PUBKEY_free(pubkey);
    return key;
}

}

std::string IdentityPublicKeyText(const HostIdentity& identity) {
    return PublicKeyTextFromSpki(identity.publicKey);
}

std::string IdentityPublicKeyLine(const HostIdentity& identity, std::string_view label) {
    auto parsed = deskhub::ParsePublicKeyText(IdentityPublicKeyText(identity));
    if (!parsed) return {};
    parsed->label = std::string(label);
    return deskhub::FormatPublicKeyText(*parsed);
}

std::string PublicKeyTextFromSpki(std::span<const uint8_t> spkiDer) {
    const PkeyPtr key = PublicKeyFromSpki(spkiDer);
    if (!key) return {};
    deskhub::PublicKeyText text;
    if (EVP_PKEY_id(key.get()) == EVP_PKEY_ED25519) {
        std::array<uint8_t, 32> bytes{};
        size_t length = bytes.size();
        if (EVP_PKEY_get_raw_public_key(key.get(), bytes.data(), &length) != 1 ||
            length != bytes.size())
            return {};
        text.algorithm = deskhub::PublicKeyAlgorithm::Ed25519;
        AppendSshString(text.blob, "ssh-ed25519");
        AppendSshString(text.blob, bytes);
        return deskhub::FormatPublicKeyText(text);
    }
    if (EVP_PKEY_id(key.get()) != EVP_PKEY_EC) return {};
    const EC_KEY* ec = EVP_PKEY_get0_EC_KEY(key.get());
    if (!ec) return {};
    const EC_GROUP* group = EC_KEY_get0_group(ec);
    const EC_POINT* point = EC_KEY_get0_public_key(ec);
    if (!group || !point || EC_GROUP_get_curve_name(group) != NID_X9_62_prime256v1)
        return {};
    const size_t length = EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
        nullptr, 0, nullptr);
    if (length != 65) return {};
    std::vector<uint8_t> bytes(length);
    if (EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED, bytes.data(),
            bytes.size(), nullptr) != length)
        return {};
    text.algorithm = deskhub::PublicKeyAlgorithm::EcdsaP256;
    AppendSshString(text.blob, "ecdsa-sha2-nistp256");
    AppendSshString(text.blob, "nistp256");
    AppendSshString(text.blob, bytes);
    return deskhub::FormatPublicKeyText(text);
}

std::vector<uint8_t> PublicKeySpkiFromText(std::string_view text) {
    const auto parsed = deskhub::ParsePublicKeyText(text);
    if (!parsed) return {};
    std::span<const uint8_t> blob(parsed->blob);
    ReadSshString(blob);
    if (parsed->algorithm == deskhub::PublicKeyAlgorithm::Ed25519) {
        const auto bytes = ReadSshString(blob);
        const PkeyPtr key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, bytes.data(),
            bytes.size()));
        return key ? SpkiFromKey(key.get()) : std::vector<uint8_t>{};
    }
    ReadSshString(blob);
    const auto bytes = ReadSshString(blob);
    const EcKeyPtr ec(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    if (!ec) return {};
    const EC_GROUP* group = EC_KEY_get0_group(ec.get());
    const EcPointPtr point(EC_POINT_new(group));
    if (!point || EC_POINT_oct2point(group, point.get(), bytes.data(), bytes.size(), nullptr) != 1 ||
        EC_KEY_set_public_key(ec.get(), point.get()) != 1 || EC_KEY_check_key(ec.get()) != 1)
        return {};
    const PkeyPtr key(EVP_PKEY_new());
    if (!key || EVP_PKEY_set1_EC_KEY(key.get(), ec.get()) != 1) return {};
    return SpkiFromKey(key.get());
}

std::optional<deskhub::Fingerprint> FingerprintOfPublicKey(std::span<const uint8_t> spkiDer) {
    if (spkiDer.empty()) return std::nullopt;
    if (!PublicKeyFromSpki(spkiDer)) return std::nullopt;
    deskhub::Fingerprint fp;
    SHA256(spkiDer.data(), spkiDer.size(), fp.bytes.data());
    return fp;
}

std::vector<uint8_t> SignWithIdentity(const HostIdentity& identity,
    std::span<const uint8_t> data) {
    const PkeyPtr key = PrivateKeyFromPem(identity.keyPem);
    if (!key) return {};
    const MdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) return {};
    const EVP_MD* digest = EVP_PKEY_id(key.get()) == EVP_PKEY_ED25519 ? nullptr : EVP_sha256();
    if (EVP_DigestSignInit(ctx.get(), nullptr, digest, nullptr, key.get()) != 1) return {};

    size_t len = 0;
    if (EVP_DigestSign(ctx.get(), nullptr, &len, data.data(), data.size()) != 1) return {};
    std::vector<uint8_t> signature(len);
    if (EVP_DigestSign(ctx.get(), signature.data(), &len, data.data(), data.size()) != 1)
        return {};
    signature.resize(len);
    return signature;
}

bool VerifySignature(std::span<const uint8_t> spkiDer, std::span<const uint8_t> data,
    std::span<const uint8_t> signature) {
    if (signature.empty()) return false;
    const PkeyPtr key = PublicKeyFromSpki(spkiDer);
    if (!key) return false;
    const MdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) return false;
    const EVP_MD* digest = EVP_PKEY_id(key.get()) == EVP_PKEY_ED25519 ? nullptr : EVP_sha256();
    if (EVP_DigestVerifyInit(ctx.get(), nullptr, digest, nullptr, key.get()) != 1)
        return false;
    return EVP_DigestVerify(ctx.get(), signature.data(), signature.size(), data.data(),
               data.size()) == 1;
}

}
