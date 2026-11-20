#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include "crypto_manager.h"
#include "media.h"
#include "net.h"

namespace chat {

// One chat user. Every encryption and decryption step happens here,
// never on the server.
class Client {
public:
    explicit Client(std::string username);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Creates the RSA key pair and opens the TCP connection
    bool connectToServer(const std::string& host, int port);
    void disconnect();

    // Announces the user name and publishes the public key
    bool registerWithServer();

    // Creates a random AES key and sends it encrypted with the peer public key
    bool startKeyExchange(const std::string& peer);

    // Encrypts and sends one message to the peer
    bool sendMessage(MessageType type, const Bytes& plaintext);
    bool sendText(const std::string& text);
    bool sendMedia(MessageType type, const Media& media);

    // Reads packets until the connection closes; runs in its own thread
    void receiveLoop();

    bool running() const { return running_.load(); }
    bool hasSessionKey() const;
    std::string peerName() const;
    std::string ownFingerprint() const;
    std::string peerFingerprint() const;

    // Folder where received voice and image files are stored
    void setDownloadDir(std::string dir);

private:
    void handlePublicKey(const Message& msg);
    void handleKeyExchange(const Message& msg);
    void handleText(const Message& msg);
    void handleMedia(const Message& msg);

    bool sendPacketLocked(const Message& msg);
    bool publishPublicKey(const std::string& receiver);

    // Keeps a received file name from pointing anywhere outside the folder
    std::string safeTargetPath(const std::string& fileName) const;

    std::string username_;
    CryptoManager crypto_;
    SocketHandle socket_ = kInvalidSocket;
    std::atomic<bool> running_{false};

    mutable std::mutex stateMutex_;
    std::string peer_;
    Bytes peerPublicKey_;
    Bytes sessionKey_;

    std::mutex sendMutex_;
    std::string downloadDir_ = "received";
};

}  // namespace chat
