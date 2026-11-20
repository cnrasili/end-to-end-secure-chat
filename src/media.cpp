#include "media.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace chat {

namespace {

// Lower case copy, used when comparing file extensions
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

}  // namespace

std::string Media::baseName(const std::string& path) {
    const size_t cut = path.find_last_of("/\\");
    return cut == std::string::npos ? path : path.substr(cut + 1);
}

std::string Media::mimeFromPath(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "application/octet-stream";
    }

    const std::string ext = toLower(path.substr(dot + 1));
    if (ext == "png")                   return "image/png";
    if (ext == "jpg" || ext == "jpeg")  return "image/jpeg";
    if (ext == "gif")                   return "image/gif";
    if (ext == "bmp")                   return "image/bmp";
    if (ext == "webp")                  return "image/webp";
    if (ext == "wav")                   return "audio/wav";
    if (ext == "mp3")                   return "audio/mpeg";
    if (ext == "ogg")                   return "audio/ogg";
    return "application/octet-stream";
}

bool Media::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (!file && !file.eof()) {
        return false;
    }

    fileName = baseName(path);
    mimeType = mimeFromPath(path);
    return true;
}

bool Media::saveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    if (!data.empty()) {
        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    }
    return static_cast<bool>(file);
}

Bytes Media::encode() const {
    const uint8_t nameLen = static_cast<uint8_t>(std::min<size_t>(fileName.size(), 255));
    const uint8_t mimeLen = static_cast<uint8_t>(std::min<size_t>(mimeType.size(), 255));

    Bytes out;
    out.reserve(2 + nameLen + mimeLen + data.size());
    out.push_back(nameLen);
    out.insert(out.end(), fileName.begin(), fileName.begin() + nameLen);
    out.push_back(mimeLen);
    out.insert(out.end(), mimeType.begin(), mimeType.begin() + mimeLen);
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

bool Media::decode(const Bytes& buffer, Media& out) {
    size_t offset = 0;
    if (buffer.size() < 1) {
        return false;
    }

    const uint8_t nameLen = buffer[offset++];
    if (buffer.size() < offset + nameLen + 1u) {
        return false;
    }
    out.fileName.assign(buffer.begin() + offset, buffer.begin() + offset + nameLen);
    offset += nameLen;

    const uint8_t mimeLen = buffer[offset++];
    if (buffer.size() < offset + mimeLen) {
        return false;
    }
    out.mimeType.assign(buffer.begin() + offset, buffer.begin() + offset + mimeLen);
    offset += mimeLen;

    out.data.assign(buffer.begin() + offset, buffer.end());
    return true;
}

}  // namespace chat
