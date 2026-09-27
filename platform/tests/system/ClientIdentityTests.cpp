#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ClientIdentity.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/nid.h>
#include <openssl/pem.h>

#include <cstdio>

namespace {

struct SavedClientKey {
    std::string key = deskhubp::ReadAppDataFile(deskhubp::kClientKeyFileName);

    SavedClientKey() {
        deskhubp::RemoveAppDataFile(deskhubp::kClientKeyFileName);
    }

    ~SavedClientKey() {
        if (key.empty())
            deskhubp::RemoveAppDataFile(deskhubp::kClientKeyFileName);
        else
            deskhubp::WriteAppDataFile(deskhubp::kClientKeyFileName, key);
    }
};

std::string EncryptedPrivateKey(std::string_view plaintext) {
    BIO* source = BIO_new_mem_buf(plaintext.data(), int(plaintext.size()));
    if (!source) return {};
    EVP_PKEY* key = PEM_read_bio_PrivateKey(source, nullptr, nullptr, nullptr);
    BIO_free(source);
    if (!key) return {};
    BIO* output = BIO_new(BIO_s_mem());
    char passphrase[] = "correct passphrase";
    const bool okay = output &&
                      PEM_write_bio_PKCS8PrivateKey(output, key, EVP_aes_256_cbc(),
                          passphrase, sizeof(passphrase) - 1, nullptr,
                          nullptr) == 1;
    EVP_PKEY_free(key);
    if (!okay) {
        BIO_free(output);
        return {};
    }
    const uint8_t* data = nullptr;
    size_t size = 0;
    BIO_mem_contents(output, &data, &size);
    const std::string pem(reinterpret_cast<const char*>(data), size);
    BIO_free(output);
    return pem;
}

std::string NewP256PrivateKey() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx) return {};
    EVP_PKEY* key = nullptr;
    const bool generated = EVP_PKEY_keygen_init(ctx) == 1 &&
                           EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) ==
                               1 &&
                           EVP_PKEY_keygen(ctx, &key) == 1;
    EVP_PKEY_CTX_free(ctx);
    if (!generated) return {};
    BIO* output = BIO_new(BIO_s_mem());
    const bool written = output &&
                         PEM_write_bio_PKCS8PrivateKey(output, key, nullptr, nullptr, 0,
                             nullptr, nullptr) == 1;
    EVP_PKEY_free(key);
    if (!written) {
        BIO_free(output);
        return {};
    }
    const uint8_t* data = nullptr;
    size_t size = 0;
    BIO_mem_contents(output, &data, &size);
    const std::string pem(reinterpret_cast<const char*>(data), size);
    BIO_free(output);
    return pem;
}

void TestClientKeyIsSeparateAndStable() {
    std::printf("[client identity] the signing key is separate and persists...\n");
    if (!deskhubp::QuicAvailable()) return;
    const SavedClientKey saved;
    const deskhubp::ClientIdentity first = deskhubp::LoadOrCreateClientIdentity();
    Check(first.Valid(), "the first use creates a signing key");
    if (!first.Valid()) return;
    Check(!deskhubp::ClientPublicKeyText(first).empty(), "its public key can be shared as text");
    Check(deskhubp::PublicKeySpkiFromText(deskhubp::ClientPublicKeyText(first)) ==
              first.publicKey,
        "the shared text identifies the exact signing key");
    const deskhubp::ClientIdentity again = deskhubp::LoadOrCreateClientIdentity();
    Check(again.Valid() && again.fingerprint == first.fingerprint,
        "subsequent loads keep the same key");

    const std::vector<uint8_t> message = {'d', 'e', 's', 'k', 'h', 'u', 'b'};
    const auto signature = deskhubp::SignWithClientIdentity(first, message);
    Check(!signature.empty() &&
              deskhubp::VerifySignature(first.publicKey, message, signature),
        "the client proves possession of its private key");
    Check(!deskhubp::ImportClientIdentity("not a key", {}), "junk cannot replace the key");
    Check(deskhubp::LoadClientIdentity().fingerprint == first.fingerprint,
        "a failed import keeps the active identity");
    Check(deskhubp::ImportClientIdentity(first.keyPem, {}),
        "a supported external PKCS#8 key can be imported");

    const std::string encrypted = EncryptedPrivateKey(first.keyPem);
    Check(!encrypted.empty(), "a password-protected PKCS#8 fixture can be made");
    Check(!deskhubp::ImportClientIdentity(encrypted, "wrong"),
        "a wrong key passphrase cannot replace the identity");
    Check(deskhubp::ImportClientIdentity(encrypted, "correct passphrase"),
        "the right passphrase unlocks an encrypted external key");

    const std::string p256 = NewP256PrivateKey();
    Check(!p256.empty() && deskhubp::ImportClientIdentity(p256, {}),
        "an external P-256 PKCS#8 key can be imported");
    const auto imported = deskhubp::LoadClientIdentity();
    Check(imported.Valid() && imported.fingerprint != first.fingerprint,
        "importing a different key changes the client identity deliberately");
    Check(deskhubp::VerifySignature(imported.publicKey, message,
              deskhubp::SignWithClientIdentity(imported, message)),
        "the imported P-256 key can authenticate a connection");
}

}

void RunClientIdentityTests() {
    TestClientKeyIsSeparateAndStable();
}
