#include "client.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <utility>

namespace chat {

Client::Client(std::string username)
    : username_(std::move(username)), crypto_(kRsaBits) {}

Client::~Client() {
    disconnect();
}

void Client::setDownloadDir(std::string dir) {
    downloadDir_ = std::move(dir);
}

bool Client::connectToServer(const std::string& host, int port) {
    std::cout << "[client] creating a " << crypto_.rsaBits() << " bit RSA key pair...\n";
    if (!crypto_.generateRsaKeyPair()) {
        std::cerr << "[client] key generation failed\n";
        return false;
    }

    socket_ = connectTo(host, port);
    if (socket_ == kInvalidSocket) {
        std::cerr << "[client] cannot reach the server at " << host << ":" << port << "\n";
        return false;
    }

    running_.store(true);
    std::cout << "[client] connected to " << host << ":" << port << "\n";
    std::cout << "[client] your key fingerprint: " << ownFingerprint() << "\n";
    return true;
}

void Client::disconnect() {
    if (socket_ != kInvalidSocket) {
        closeSocket(socket_);
        socket_ = kInvalidSocket;
    }
    running_.store(false);
}

bool Client::sendPacketLocked(const Message& msg) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (socket_ == kInvalidSocket) {
        return false;
    }
    return sendPacket(socket_, msg);
}

bool Client::registerWithServer() {
    const Message hello(MessageType::Register, username_, "", {});
    if (!sendPacketLocked(hello)) {
        std::cerr << "[client] could not register\n";
        return false;
    }

    // An empty receiver means the server passes this to whoever else is online
    return publishPublicKey("");
}

bool Client::publishPublicKey(const std::string& receiver) {
    const Bytes der = crypto_.publicKeyDer();
    if (der.empty()) {
        return false;
    }
    return sendPacketLocked(Message(MessageType::PublicKey, username_, receiver, der));
}

std::string Client::ownFingerprint() const {
    return CryptoManager::fingerprint(crypto_.publicKeyDer());
}

std::string Client::peerFingerprint() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return CryptoManager::fingerprint(peerPublicKey_);
}

std::string Client::peerName() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return peer_;
}

bool Client::hasSessionKey() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return sessionKey_.size() == kAesKeySize;
}

bool Client::startKeyExchange(const std::string& peer) {
    Bytes peerKey;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (peer_ != peer || peerPublicKey_.empty()) {
            std::cerr << "[client] the public key of " << peer << " is not known yet\n";
            return false;
        }
        if (sessionKey_.size() == kAesKeySize) {
            return true;
        }
        peerKey = peerPublicKey_;
    }

    const Bytes key = CryptoManager::randomBytes(kAesKeySize);
    if (key.size() != kAesKeySize) {
        std::cerr << "[client] could not create an AES key\n";
        return false;
    }

    Bytes wrapped;
    if (!CryptoManager::rsaEncrypt(peerKey, key, wrapped)) {
        std::cerr << "[client] could not encrypt the AES key\n";
        return false;
    }

    if (!sendPacketLocked(Message(MessageType::KeyExchange, username_, peer, wrapped))) {
        std::cerr << "[client] could not send the AES key\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        sessionKey_ = key;
    }

    std::cout << "[client] session key created and sent to " << peer << "\n";
    return true;
}

bool Client::sendMessage(MessageType type, const Bytes& plaintext) {
    Bytes key;
    std::string peer;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        key = sessionKey_;
        peer = peer_;
    }

    if (key.size() != kAesKeySize) {
        std::cerr << "[client] no session key yet, nothing was sent\n";
        return false;
    }
    if (peer.empty()) {
        std::cerr << "[client] nobody to talk to yet\n";
        return false;
    }

    Bytes cipher;
    if (!CryptoManager::aesGcmEncrypt(key, plaintext, cipher)) {
        std::cerr << "[client] encryption failed\n";
        return false;
    }

    return sendPacketLocked(Message(type, username_, peer, cipher));
}

bool Client::sendText(const std::string& text) {
    const Bytes plain(text.begin(), text.end());
    return sendMessage(MessageType::Text, plain);
}

bool Client::sendMedia(MessageType type, const Media& media) {
    // The file name and type are encrypted together with the content
    return sendMessage(type, media.encode());
}

void Client::receiveLoop() {
    Message msg;
    while (running_.load() && socket_ != kInvalidSocket && receivePacket(socket_, msg)) {
        switch (msg.type) {
            case MessageType::PublicKey:   handlePublicKey(msg); break;
            case MessageType::KeyExchange: handleKeyExchange(msg); break;
            case MessageType::Text:        handleText(msg); break;
            case MessageType::Voice:
            case MessageType::Image:       handleMedia(msg); break;
            default:
                std::cout << "\n[client] ignoring a "
                          << messageTypeName(msg.type) << " packet\n";
                break;
        }
    }

    if (running_.load()) {
        std::cout << "\n[client] the connection was closed\n";
        running_.store(false);
    }
}

void Client::handlePublicKey(const Message& msg) {
    bool isNewPeer = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (peerPublicKey_.empty()) {
            peer_ = msg.sender;
            peerPublicKey_ = msg.payload;
            isNewPeer = true;
        } else if (msg.sender != peer_) {
            std::cout << "\n[client] " << msg.sender
                      << " also wants to chat, but this build handles one peer only\n";
            return;
        }
    }

    if (!isNewPeer) {
        return;
    }

    std::cout << "\n[client] " << msg.sender << " is online\n";
    std::cout << "[client] their key fingerprint: " << peerFingerprint() << "\n";
    std::cout << "[client] compare it with " << msg.sender
              << " before trusting the chat\n";

    // Answer with our own key so both sides can check a fingerprint
    publishPublicKey(msg.sender);

    // Only one side creates the key, so the two clients cannot overwrite
    // each other. The smaller user name starts it.
    if (username_ < msg.sender) {
        startKeyExchange(msg.sender);
    } else {
        std::cout << "[client] waiting for " << msg.sender
                  << " to send the session key\n";
    }
}

void Client::handleKeyExchange(const Message& msg) {
    Bytes key;
    if (!crypto_.rsaDecrypt(msg.payload, key) || key.size() != kAesKeySize) {
        std::cerr << "\n[client] the session key from " << msg.sender
                  << " could not be read\n";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        sessionKey_ = key;
        if (peer_.empty()) {
            peer_ = msg.sender;
        }
    }

    std::cout << "\n[client] session key received from " << msg.sender
              << ", the chat is encrypted now\n";
}

void Client::handleText(const Message& msg) {
    Bytes key;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        key = sessionKey_;
    }

    Bytes plain;
    if (!CryptoManager::aesGcmDecrypt(key, msg.payload, plain)) {
        // A wrong tag means the packet was changed, so it is thrown away
        std::cerr << "\n[client] a message from " << msg.sender
                  << " failed the tag check and was discarded\n";
        return;
    }

    std::cout << "\n" << msg.sender << ": "
              << std::string(plain.begin(), plain.end()) << "\n";
}

std::string Client::safeTargetPath(const std::string& fileName) const {
    std::string name = Media::baseName(fileName);
    // Keeps a crafted name from escaping the download folder
    name.erase(std::remove_if(name.begin(), name.end(),
                              [](char c) {
                                  return c == ':' || c == '/' || c == '\\' ||
                                         c == '*' || c == '?' || c == '"' ||
                                         c == '<' || c == '>' || c == '|';
                              }),
               name.end());

    while (!name.empty() && name.front() == '.') {
        name.erase(name.begin());
    }
    if (name.empty()) {
        name = "received_file";
    }

    std::filesystem::path target = std::filesystem::path(downloadDir_) / name;

    // Never overwrites a file that is already there
    int counter = 1;
    while (std::filesystem::exists(target)) {
        const std::filesystem::path base(name);
        const std::string stem = base.stem().string();
        const std::string ext = base.extension().string();
        target = std::filesystem::path(downloadDir_) /
                 (stem + "_" + std::to_string(counter++) + ext);
    }
    return target.string();
}

void Client::handleMedia(const Message& msg) {
    Bytes key;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        key = sessionKey_;
    }

    Bytes plain;
    if (!CryptoManager::aesGcmDecrypt(key, msg.payload, plain)) {
        std::cerr << "\n[client] a file from " << msg.sender
                  << " failed the tag check and was discarded\n";
        return;
    }

    Media media;
    if (!Media::decode(plain, media)) {
        std::cerr << "\n[client] a file from " << msg.sender
                  << " could not be read\n";
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(downloadDir_, error);

    const std::string path = safeTargetPath(media.fileName);
    if (!media.saveToFile(path)) {
        std::cerr << "\n[client] could not write " << path << "\n";
        return;
    }

    std::cout << "\n" << msg.sender
              << (msg.type == MessageType::Voice ? " sent a voice message" : " sent an image")
              << " (" << media.data.size() << " bytes, " << media.mimeType << ")\n"
              << "[client] saved to " << path << "\n";
}

}  // namespace chat
