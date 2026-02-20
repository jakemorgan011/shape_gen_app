#pragma once

#include <RtAudio.h>
#include <vector>
#include <atomic>
#include <memory>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize(unsigned int sampleRate = 44100, unsigned int bufferSize = 512);
    void start();
    void stop();

    bool isRunning() const { return m_isRunning; }
    unsigned int getSampleRate() const { return m_sampleRate; }
    unsigned int getBufferSize() const { return m_bufferSize; }

private:
    static int audioCallback(void* outputBuffer, void* inputBuffer,
                           unsigned int nFrames, double streamTime,
                           RtAudioStreamStatus status, void* userData);

    void processAudio(float* output, float* input, unsigned int nFrames);

    std::unique_ptr<RtAudio> m_audio;
    std::atomic<bool> m_isRunning;

    unsigned int m_sampleRate;
    unsigned int m_bufferSize;

    // DSP state - example oscillator
    double m_phase;
    double m_frequency;
};
