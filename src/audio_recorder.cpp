#include "audio_recorder.h"

#include <portaudio.h>

#include <cstring>
#include <iostream>
#include <vector>

namespace chat {

namespace {

constexpr int kFramesPerBuffer = 512;

// Appends a little endian value of the given width
void append(Bytes& out, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

void appendTag(Bytes& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

}  // namespace

Bytes AudioRecorder::buildWav(const Bytes& pcm, int sampleRate, int channels) {
    const uint32_t dataSize = static_cast<uint32_t>(pcm.size());
    const uint32_t byteRate = static_cast<uint32_t>(sampleRate * channels * 2);

    Bytes wav;
    wav.reserve(44 + pcm.size());

    appendTag(wav, "RIFF");
    append(wav, 36 + dataSize, 4);
    appendTag(wav, "WAVE");

    appendTag(wav, "fmt ");
    append(wav, 16, 4);                                    // chunk size
    append(wav, 1, 2);                                     // PCM
    append(wav, static_cast<uint32_t>(channels), 2);
    append(wav, static_cast<uint32_t>(sampleRate), 4);
    append(wav, byteRate, 4);
    append(wav, static_cast<uint32_t>(channels * 2), 2);   // block align
    append(wav, 16, 2);                                    // bits per sample

    appendTag(wav, "data");
    append(wav, dataSize, 4);
    wav.insert(wav.end(), pcm.begin(), pcm.end());
    return wav;
}

bool AudioRecorder::record(int seconds, Bytes& wavOut) {
    if (seconds <= 0) {
        return false;
    }

    PaError status = Pa_Initialize();
    if (status != paNoError) {
        std::cerr << "[audio] cannot start PortAudio: " << Pa_GetErrorText(status) << "\n";
        return false;
    }

    PaStream* stream = nullptr;
    status = Pa_OpenDefaultStream(&stream, kChannels, 0, paInt16,
                                  kSampleRate, kFramesPerBuffer, nullptr, nullptr);
    if (status != paNoError) {
        std::cerr << "[audio] cannot open the microphone: "
                  << Pa_GetErrorText(status) << "\n";
        Pa_Terminate();
        return false;
    }

    status = Pa_StartStream(stream);
    if (status != paNoError) {
        std::cerr << "[audio] cannot start recording: " << Pa_GetErrorText(status) << "\n";
        Pa_CloseStream(stream);
        Pa_Terminate();
        return false;
    }

    const long totalFrames = static_cast<long>(kSampleRate) * seconds;
    std::vector<int16_t> block(static_cast<size_t>(kFramesPerBuffer) * kChannels);

    Bytes pcm;
    pcm.reserve(static_cast<size_t>(totalFrames) * kChannels * 2);

    std::cout << "[audio] recording for " << seconds << " seconds...\n";

    for (long done = 0; done < totalFrames; done += kFramesPerBuffer) {
        const long frames = (totalFrames - done < kFramesPerBuffer)
                                ? (totalFrames - done)
                                : kFramesPerBuffer;

        status = Pa_ReadStream(stream, block.data(), static_cast<unsigned long>(frames));
        // An overflow only means some samples were lost, the recording goes on
        if (status != paNoError && status != paInputOverflowed) {
            std::cerr << "[audio] read error: " << Pa_GetErrorText(status) << "\n";
            break;
        }

        const uint8_t* raw = reinterpret_cast<const uint8_t*>(block.data());
        pcm.insert(pcm.end(), raw, raw + static_cast<size_t>(frames) * kChannels * 2);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    if (pcm.empty()) {
        return false;
    }

    wavOut = buildWav(pcm, kSampleRate, kChannels);
    std::cout << "[audio] recorded " << wavOut.size() << " bytes\n";
    return true;
}

}  // namespace chat
