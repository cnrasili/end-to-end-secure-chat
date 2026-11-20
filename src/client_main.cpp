#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "audio_recorder.h"
#include "client.h"
#include "media.h"

namespace {

void printHelp() {
    std::cout << "\ncommands:\n"
              << "  /voice [seconds]   record and send a voice message (5 by default)\n"
              << "  /image <path>      send an image file\n"
              << "  /fp                show the key fingerprints this side knows\n"
              << "  /help              show this list\n"
              << "  /quit              leave the chat\n"
              << "  anything else is sent as a text message\n\n";
}

// Splits "/image my photo.png" into the command and the rest of the line
void splitCommand(const std::string& line, std::string& command, std::string& rest) {
    const size_t space = line.find(' ');
    if (space == std::string::npos) {
        command = line;
        rest.clear();
        return;
    }
    command = line.substr(0, space);
    rest = line.substr(space + 1);
    while (!rest.empty() && rest.front() == ' ') {
        rest.erase(rest.begin());
    }
    while (!rest.empty() && rest.back() == ' ') {
        rest.pop_back();
    }
}

// Removes quotes a user may have pasted around a path
std::string stripQuotes(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

void sendVoice(chat::Client& client, const std::string& argument) {
    int seconds = 5;
    if (!argument.empty()) {
        seconds = std::atoi(argument.c_str());
        if (seconds <= 0 || seconds > 60) {
            std::cout << "[client] pick a length between 1 and 60 seconds\n";
            return;
        }
    }

    chat::Bytes wav;
    if (!chat::AudioRecorder::record(seconds, wav)) {
        std::cout << "[client] recording failed\n";
        return;
    }

    chat::Media media;
    media.fileName = "voice_" + std::to_string(std::time(nullptr)) + ".wav";
    media.mimeType = "audio/wav";
    media.data = wav;

    if (client.sendMedia(chat::MessageType::Voice, media)) {
        std::cout << "[client] voice message sent (" << media.data.size() << " bytes)\n";
    }
}

void sendImage(chat::Client& client, const std::string& argument) {
    const std::string path = stripQuotes(argument);
    if (path.empty()) {
        std::cout << "[client] usage: /image <path>\n";
        return;
    }

    chat::Media media;
    if (!media.loadFromFile(path)) {
        std::cout << "[client] cannot read " << path << "\n";
        return;
    }

    if (media.data.size() > chat::kMaxPayloadSize / 2) {
        std::cout << "[client] that file is too large for one message\n";
        return;
    }

    if (client.sendMedia(chat::MessageType::Image, media)) {
        std::cout << "[client] image sent (" << media.data.size() << " bytes)\n";
    }
}

// Runs one chat session. The Client lives for the whole function, so its socket
// is closed only after the receive thread has been joined.
int runClient(const std::string& username, const std::string& host, int port) {
    chat::Client client(username);
    client.setDownloadDir("received_" + username);

    if (!client.connectToServer(host, port) || !client.registerWithServer()) {
        return 1;
    }

    std::thread receiver(&chat::Client::receiveLoop, &client);

    std::cout << "[client] waiting for the other user to come online\n";
    printHelp();

    std::string line;
    while (client.running() && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        if (line[0] == '/') {
            std::string command;
            std::string argument;
            splitCommand(line, command, argument);

            if (command == "/quit") {
                break;
            }
            if (command == "/help") {
                printHelp();
                continue;
            }
            if (command == "/fp") {
                std::cout << "[client] your key:  " << client.ownFingerprint() << "\n";
                const std::string peer = client.peerName();
                const std::string peerPrint = client.peerFingerprint();
                if (peer.empty()) {
                    std::cout << "[client] no peer yet\n";
                } else if (peerPrint.empty()) {
                    // Only the user who joined first is sent a public key
                    std::cout << "[client] the key of " << peer
                              << " did not reach this side, ask them to read"
                                 " out your fingerprint instead\n";
                } else {
                    std::cout << "[client] " << peer << ": " << peerPrint << "\n";
                }
                continue;
            }
            if (command == "/voice") {
                sendVoice(client, argument);
                continue;
            }
            if (command == "/image") {
                sendImage(client, argument);
                continue;
            }

            std::cout << "[client] unknown command, try /help\n";
            continue;
        }

        client.sendText(line);
    }

    client.disconnect();
    if (receiver.joinable()) {
        receiver.join();
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: chat_client <username> [host] [port]\n";
        return 1;
    }

    const std::string username = argv[1];
    if (username.size() > 32) {
        std::cerr << "the user name can be at most 32 characters\n";
        return 1;
    }

    const std::string host = (argc > 2) ? argv[2] : "127.0.0.1";
    const int port = (argc > 3) ? std::atoi(argv[3]) : chat::kDefaultPort;

    if (!chat::netStartup()) {
        std::cerr << "[client] socket library failed to start\n";
        return 1;
    }

    const int result = runClient(username, host, port);

    chat::netCleanup();
    std::cout << "[client] bye\n";
    return result;
}
