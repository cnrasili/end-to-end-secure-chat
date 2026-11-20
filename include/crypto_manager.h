#pragma once

#include <string>

#include "message.h"

// Avoids pulling the OpenSSL headers into every translation unit
struct evp_pkey_st;

namespace chat {

// All encryption and decryption happens here, on the client side only
class CryptoManager {
public:
    explicit CryptoManager(int rsaBits = kRsaBits);
    ~CryptoManager();

    CryptoManager(const CryptoManager&) = delete;
    CryptoManager& operator=(const CryptoManager&) = delete;

    // Creates this client's own RSA key pair
    bool generateRsaKeyPair();

    // This client's public key in DER form, ready to be sent to the peer
    Bytes publicKeyDer() const;

    // Encrypts a short buffer (the AES key) with the peer public key
    static bool rsaEncrypt(const Bytes& peerPublicKeyDer, const Bytes& data, Bytes& out);

    // Decrypts with this client's private key
    bool rsaDecrypt(const Bytes& data, Bytes& out) const;

    // Produces IV || ciphertext || tag
    static bool aesGcmEncrypt(const Bytes& key, const Bytes& plain, Bytes& out);

    // Checks the tag and returns the plaintext, or fails if the data was changed
    static bool aesGcmDecrypt(const Bytes& key, const Bytes& packet, Bytes& out);

    // Random bytes from the OpenSSL generator, used for the AES key and the IV
    static Bytes randomBytes(size_t count);

    // SHA-256 of a public key, grouped so two users can compare it by voice
    static std::string fingerprint(const Bytes& publicKeyDer);

    int rsaBits() const { return rsaBits_; }

private:
    int rsaBits_;
    evp_pkey_st* keyPair_ = nullptr;
};

}  // namespace chat
