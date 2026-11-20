#pragma once

#include <string>

#include "message.h"

namespace chat {

// Voice recording or image carried inside an encrypted message
class Media {
public:
    std::string fileName;
    std::string mimeType;
    Bytes data;

    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;

    // Packs the file name and type together with the bytes, so that all of it
    // is encrypted and the relay server sees none of it
    Bytes encode() const;
    static bool decode(const Bytes& buffer, Media& out);

    // Picks a MIME type from the file extension
    static std::string mimeFromPath(const std::string& path);

    // Strips the directory part of a path
    static std::string baseName(const std::string& path);
};

}  // namespace chat
