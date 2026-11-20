#pragma once

#include <string>

#include "message.h"

// Avoids pulling the OpenSSL headers into every translation unit
struct evp_pkey_st;

namespace chat {

// The AES session key the two clients share
using AesKey = Bytes;

// An RSA key pair, or only a public key when it came from the peer
class RsaKey {
public:
    RsaKey() = default;
    ~RsaKey();

    RsaKey(RsaKey&& other) noexcept;
    RsaKey& operator=(RsaKey&& other) noexcept;
    RsaKey(const RsaKey&) = delete;
    RsaKey& operator=(const RsaKey&) = delete;

    bool valid() const { return pkey_ != nullptr; }
    void reset();

    // The public half in DER form, which is what travels to the peer
    Bytes publicDer() const;

    // Rebuilds a public key received from the peer
    static bool fromPublicDer(const Bytes& der, RsaKey& out);

private:
    friend class CryptoManager;
    evp_pkey_st* pkey_ = nullptr;
};

// Encryption helpers. The keys belong to the Client, so every call is given
// the key it should work with.
class CryptoManager {
public:
    explicit CryptoManager(int rsaBits = kRsaBits);

    // Creates a new key pair of rsaBits length
    bool generateRsaKeyPair(RsaKey& out) const;

    // Encrypts a short buffer (the AES key) with the peer public key
    static bool rsaEncrypt(const RsaKey& key, const Bytes& data, Bytes& out);

    // Decrypts with the own private key
    static bool rsaDecrypt(const RsaKey& key, const Bytes& data, Bytes& out);

    // Produces IV || ciphertext || tag
    static bool aesGcmEncrypt(const AesKey& key, const Bytes& plain, Bytes& out);

    // Checks the tag and returns the plaintext, or fails if the data was changed
    static bool aesGcmDecrypt(const AesKey& key, const Bytes& packet, Bytes& out);

    // SHA-256 of a public key, grouped so two users can compare it by voice
    static std::string fingerprint(const RsaKey& key);

    // A fresh random session key
    static AesKey randomAesKey();

    // Random bytes from the OpenSSL generator
    static Bytes randomBytes(size_t count);

    int rsaBits() const { return rsaBits_; }

private:
    int rsaBits_;
};

}  // namespace chat
