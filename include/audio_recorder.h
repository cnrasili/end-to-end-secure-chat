#pragma once

#include "message.h"

namespace chat {

// Records a short voice message from the default microphone with PortAudio
class AudioRecorder {
public:
    static constexpr int kSampleRate = 16000;
    static constexpr int kChannels = 1;

    // Records for the given number of seconds and returns a WAV file in memory
    static bool record(int seconds, Bytes& wavOut);

    // Wraps raw 16 bit samples in a WAV header so any player can open the file
    static Bytes buildWav(const Bytes& pcm, int sampleRate, int channels);
};

}  // namespace chat
