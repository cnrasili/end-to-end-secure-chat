#include "crypto_manager.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include <cstdio>
#include <utility>

namespace chat {

namespace {

// Applies OAEP with SHA-256 to an RSA context
bool configureOaep(EVP_PKEY_CTX* ctx) {
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        return false;
    }
    return EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) > 0;
}

}  // namespace

RsaKey::~RsaKey() {
    reset();
}

RsaKey::RsaKey(RsaKey&& other) noexcept : pkey_(other.pkey_) {
    other.pkey_ = nullptr;
}

RsaKey& RsaKey::operator=(RsaKey&& other) noexcept {
    if (this != &other) {
        reset();
        pkey_ = other.pkey_;
        other.pkey_ = nullptr;
    }
    return *this;
}

void RsaKey::reset() {
    if (pkey_ != nullptr) {
        EVP_PKEY_free(reinterpret_cast<EVP_PKEY*>(pkey_));
        pkey_ = nullptr;
    }
}

Bytes RsaKey::publicDer() const {
    if (pkey_ == nullptr) {
        return {};
    }

    unsigned char* buffer = nullptr;
    const int length = i2d_PUBKEY(reinterpret_cast<EVP_PKEY*>(pkey_), &buffer);
    if (length <= 0 || buffer == nullptr) {
        return {};
    }

    Bytes der(buffer, buffer + length);
    OPENSSL_free(buffer);
    return der;
}

bool RsaKey::fromPublicDer(const Bytes& der, RsaKey& out) {
    if (der.empty()) {
        return false;
    }

    const unsigned char* cursor = der.data();
    EVP_PKEY* parsed = d2i_PUBKEY(nullptr, &cursor, static_cast<long>(der.size()));
    if (parsed == nullptr) {
        return false;
    }

    out.reset();
    out.pkey_ = reinterpret_cast<evp_pkey_st*>(parsed);
    return true;
}

CryptoManager::CryptoManager(int rsaBits) : rsaBits_(rsaBits) {}

bool CryptoManager::generateRsaKeyPair(RsaKey& out) const {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (ctx == nullptr) {
        return false;
    }

    EVP_PKEY* created = nullptr;
    const bool ok = EVP_PKEY_keygen_init(ctx) > 0 &&
                    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, rsaBits_) > 0 &&
                    EVP_PKEY_keygen(ctx, &created) > 0;
    EVP_PKEY_CTX_free(ctx);

    if (!ok) {
        return false;
    }

    out.reset();
    out.pkey_ = reinterpret_cast<evp_pkey_st*>(created);
    return true;
}

bool CryptoManager::rsaEncrypt(const RsaKey& key, const Bytes& data, Bytes& out) {
    if (!key.valid()) {
        return false;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(reinterpret_cast<EVP_PKEY*>(key.pkey_), nullptr);
    if (ctx == nullptr) {
        return false;
    }

    size_t length = 0;
    bool ok = EVP_PKEY_encrypt_init(ctx) > 0 &&
              configureOaep(ctx) &&
              EVP_PKEY_encrypt(ctx, nullptr, &length, data.data(), data.size()) > 0;

    if (ok) {
        out.resize(length);
        ok = EVP_PKEY_encrypt(ctx, out.data(), &length, data.data(), data.size()) > 0;
        if (ok) {
            out.resize(length);
        }
    }

    EVP_PKEY_CTX_free(ctx);
    if (!ok) {
        out.clear();
    }
    return ok;
}

bool CryptoManager::rsaDecrypt(const RsaKey& key, const Bytes& data, Bytes& out) {
    if (!key.valid()) {
        return false;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(reinterpret_cast<EVP_PKEY*>(key.pkey_), nullptr);
    if (ctx == nullptr) {
        return false;
    }

    size_t length = 0;
    bool ok = EVP_PKEY_decrypt_init(ctx) > 0 &&
              configureOaep(ctx) &&
              EVP_PKEY_decrypt(ctx, nullptr, &length, data.data(), data.size()) > 0;

    if (ok) {
        out.resize(length);
        ok = EVP_PKEY_decrypt(ctx, out.data(), &length, data.data(), data.size()) > 0;
        if (ok) {
            out.resize(length);
        }
    }

    EVP_PKEY_CTX_free(ctx);
    if (!ok) {
        out.clear();
    }
    return ok;
}

Bytes CryptoManager::randomBytes(size_t count) {
    Bytes buffer(count);
    if (count > 0 && RAND_bytes(buffer.data(), static_cast<int>(count)) != 1) {
        return {};
    }
    return buffer;
}

AesKey CryptoManager::randomAesKey() {
    return randomBytes(kAesKeySize);
}

bool CryptoManager::aesGcmEncrypt(const AesKey& key, const Bytes& plain, Bytes& out) {
    if (key.size() != kAesKeySize) {
        return false;
    }

    const Bytes iv = randomBytes(kGcmIvSize);
    if (iv.size() != kGcmIvSize) {
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return false;
    }

    // The packet is laid out as IV || ciphertext || tag
    out.assign(iv.begin(), iv.end());
    out.resize(kGcmIvSize + plain.size() + kGcmTagSize);

    int length = 0;
    int cipherLength = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                                  static_cast<int>(kGcmIvSize), nullptr) == 1 &&
              EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) == 1;

    if (ok && !plain.empty()) {
        ok = EVP_EncryptUpdate(ctx, out.data() + kGcmIvSize, &length,
                               plain.data(), static_cast<int>(plain.size())) == 1;
        cipherLength = length;
    }

    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx, out.data() + kGcmIvSize + cipherLength, &length) == 1;
        cipherLength += length;
    }

    if (ok) {
        // The tag lets the receiver detect any change made on the way
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG,
                                 static_cast<int>(kGcmTagSize),
                                 out.data() + kGcmIvSize + cipherLength) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        out.clear();
        return false;
    }

    out.resize(kGcmIvSize + static_cast<size_t>(cipherLength) + kGcmTagSize);
    return true;
}

bool CryptoManager::aesGcmDecrypt(const AesKey& key, const Bytes& packet, Bytes& out) {
    if (key.size() != kAesKeySize || packet.size() < kGcmIvSize + kGcmTagSize) {
        return false;
    }

    const uint8_t* iv = packet.data();
    const uint8_t* cipher = packet.data() + kGcmIvSize;
    const size_t cipherLength = packet.size() - kGcmIvSize - kGcmTagSize;
    uint8_t* tag = const_cast<uint8_t*>(packet.data() + kGcmIvSize + cipherLength);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        return false;
    }

    out.resize(cipherLength);

    int length = 0;
    int plainLength = 0;
    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                                  static_cast<int>(kGcmIvSize), nullptr) == 1 &&
              EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) == 1;

    if (ok && cipherLength > 0) {
        ok = EVP_DecryptUpdate(ctx, out.data(), &length, cipher,
                               static_cast<int>(cipherLength)) == 1;
        plainLength = length;
    }

    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                                 static_cast<int>(kGcmTagSize), tag) == 1;
    }

    if (ok) {
        // Fails when the tag does not match, so changed data is never shown
        ok = EVP_DecryptFinal_ex(ctx, out.data() + plainLength, &length) == 1;
        plainLength += length;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        out.clear();
        return false;
    }

    out.resize(static_cast<size_t>(plainLength));
    return true;
}

std::string CryptoManager::fingerprint(const RsaKey& key) {
    const Bytes der = key.publicDer();
    if (der.empty()) {
        return {};
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(der.data(), der.size(), digest);

    // Grouped in blocks of four so the two users can read it out and compare
    std::string text;
    char pair[3];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        std::snprintf(pair, sizeof(pair), "%02X", digest[i]);
        text += pair;
        if (i % 2 == 1 && i != SHA256_DIGEST_LENGTH - 1) {
            text += ' ';
        }
    }
    return text;
}

}  // namespace chat
