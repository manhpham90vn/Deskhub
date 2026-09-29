#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/Random.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/nid.h>
#include <openssl/pem.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#ifndef _WIN32
#include <sys/stat.h>
#endif

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

std::string OpenSshFixture(std::string_view name) {
    const auto path = std::filesystem::path(DESKHUB_TEST_FIXTURES_DIR) / "openssh" /
                      std::string(name);
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void TestOpenSshPrivateKeyImport() {
    std::printf("[client identity] OpenSSH Ed25519 and P-256 private keys import...\n");
    if (!deskhubp::QuicAvailable()) return;
    const SavedClientKey saved;
    const auto original = deskhubp::LoadOrCreateClientIdentity();
    Check(original.Valid(), "a baseline identity exists before OpenSSH import");
    if (!original.Valid()) return;

    const auto checkImport = [](std::string_view fixture, std::string_view passphrase) {
        const std::string privateKey = OpenSshFixture(fixture);
        const std::string publicKey = OpenSshFixture(std::string(fixture) + ".pub");
        Check(!privateKey.empty() && !publicKey.empty(), "OpenSSH fixture files can be read");
        if (privateKey.empty() || publicKey.empty()) return;
        Check(deskhubp::ImportClientIdentity(privateKey, passphrase),
            "a supported OpenSSH private key imports");
        const auto imported = deskhubp::LoadClientIdentity();
        Check(imported.Valid() &&
                  imported.publicKey == deskhubp::PublicKeySpkiFromText(publicKey),
            "the imported private key matches the ssh-keygen public key");
        const std::vector<uint8_t> message = {'o', 'p', 'e', 'n', 's', 's', 'h'};
        Check(imported.Valid() && deskhubp::VerifySignature(imported.publicKey, message,
                                      deskhubp::SignWithClientIdentity(imported, message)),
            "the imported OpenSSH key signs an authentication proof");
    };

    checkImport("ed25519", {});
    checkImport("ed25519_encrypted", "correct passphrase");
    checkImport("p256", {});
    checkImport("p256_encrypted", "correct passphrase");
    const auto latest = deskhubp::LoadClientIdentity();
    const auto encrypted = OpenSshFixture("ed25519_encrypted");
    Check(!deskhubp::ImportClientIdentity(encrypted, {}),
        "an encrypted OpenSSH key requires its passphrase");
    Check(!deskhubp::ImportClientIdentity(encrypted, "wrong"),
        "a wrong OpenSSH passphrase is refused");
    Check(!deskhubp::ImportClientIdentity(OpenSshFixture("rsa_unsupported"), {}),
        "an unsupported OpenSSH RSA key is refused");
    Check(!deskhubp::ImportClientIdentity(OpenSshFixture("ed25519_mismatched"), {}),
        "an OpenSSH private key with a mismatched public key is refused");
    const auto plain = OpenSshFixture("ed25519");
    Check(plain.size() > 100 && !deskhubp::ImportClientIdentity(plain.substr(0, 100), {}),
        "a truncated OpenSSH key is refused");
    Check(deskhubp::LoadClientIdentity().fingerprint == latest.fingerprint,
        "failed OpenSSH imports preserve the active identity");
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

void TestNamedClientKeysStaySeparate() {
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) {
        Check(false, "the named identity test has a unique directory");
        return;
    }
    std::string name = "deskhub-client-keys-";
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    const auto dir = std::filesystem::temp_directory_path() / name;
    const std::string previous = deskhubp::AppDataDirRef();
    deskhubp::SetAppDataDir(dir.string());

    const auto first = deskhubp::LoadOrCreateClientIdentity();
    const auto second = deskhubp::GenerateClientIdentity("laptop-a");
    Check(first.Valid() && second.Valid() && first.fingerprint != second.fingerprint,
        "a named identity has its own signing key beside the default");
    Check(deskhubp::LoadClientIdentity("laptop-a").fingerprint == second.fingerprint,
        "the named identity survives a fresh load");
#ifndef _WIN32
    const auto keyPath = deskhubp::AppDataFilePath("client_key.laptop-a.pem");
    struct stat keyStat{};
    Check(::stat(keyPath.c_str(), &keyStat) == 0 && (keyStat.st_mode & 0777) == 0600,
        "a named private key is stored with owner-only permissions");
    Check(::chmod(keyPath.c_str(), 0644) == 0,
        "the test can make a private key readable by other accounts");
    Check(!deskhubp::LoadClientIdentity("laptop-a").Valid(),
        "a private key with broad permissions is refused");
    Check(::chmod(keyPath.c_str(), 0600) == 0 &&
              deskhubp::LoadClientIdentity("laptop-a").Valid(),
        "restoring owner-only permissions restores the same key");
#endif
    Check(!deskhubp::GenerateClientIdentity("laptop-a").Valid(),
        "generation never replaces an existing named key");
    Check(!deskhubp::GenerateClientIdentity("../escape").Valid(),
        "a key name cannot escape the app data directory");

    const std::string p256 = NewP256PrivateKey();
    Check(!p256.empty() && deskhubp::ImportClientIdentity("phone", p256, {}),
        "a second named identity can import a P-256 private key");
    Check(!deskhubp::ImportClientIdentity("phone", first.keyPem, {}),
        "import does not silently replace a named key");
    Check(deskhubp::LoadClientIdentity().fingerprint == first.fingerprint,
        "named imports do not change the default identity");
    const auto listed = deskhubp::ListClientIdentities();
    Check(listed.size() == 3 && listed[0].name == "default" &&
              listed[1].name == "laptop-a" && listed[2].name == "phone",
        "listing returns all identities in name order");
    Check(listed.size() == 3 && listed[1].valid && listed[2].valid &&
              !listed[2].publicKeyText.empty(),
        "listing exposes public keys without exporting private PEM");

    Check(deskhubp::WriteAppDataFile("client_key.phone.pem", "damaged private key"),
        "the named identity can be made unreadable for the regression check");
    Check(!deskhubp::LoadClientIdentity("phone").Valid(),
        "an unreadable named key is not regenerated or replaced");
    const auto damaged = deskhubp::ListClientIdentities();
    Check(damaged.size() == 3 && damaged[2].name == "phone" && !damaged[2].valid,
        "listing identifies an unusable named key without hiding it");

    deskhubp::SetAppDataDir(previous);
    std::error_code error;
    std::filesystem::remove_all(dir, error);
}

}

void RunClientIdentityTests() {
    TestClientKeyIsSeparateAndStable();
    TestOpenSshPrivateKeyImport();
    TestNamedClientKeysStaySeparate();
}
