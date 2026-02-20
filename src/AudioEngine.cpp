#include "AudioEngine.h"
#include <iostream>
#include <cmath>

AudioEngine::AudioEngine()
    : m_isRunning(false),
      m_sampleRate(44100),
      m_bufferSize(512),
      m_phase(0.0),
      m_frequency(440.0) {
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::initialize(unsigned int sampleRate, unsigned int bufferSize) {
    m_sampleRate = sampleRate;
    m_bufferSize = bufferSize;

    try {
        m_audio = std::make_unique<RtAudio>();

        if (m_audio->getDeviceCount() < 1) {
            std::cerr << "No audio devices found!" << std::endl;
            return false;
        }

        RtAudio::StreamParameters parameters;
        parameters.deviceId = m_audio->getDefaultOutputDevice();
        parameters.nChannels = 2; // stereo
        parameters.firstChannel = 0;

        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_SCHEDULE_REALTIME;

        m_audio->openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
                          m_sampleRate, &m_bufferSize,
                          &AudioEngine::audioCallback, this, &options);

        std::cout << "Audio initialized: " << m_sampleRate << " Hz, "
                  << m_bufferSize << " samples buffer" << std::endl;

        return true;

    } catch (RtAudioErrorType e) {
        std::cerr << "RtAudio error code: " << e << std::endl;
        return false;
    }
}

void AudioEngine::start() {
    if (m_audio && !m_isRunning) {
        try {
            m_audio->startStream();
            m_isRunning = true;
            std::cout << "Audio stream started" << std::endl;
        } catch (RtAudioErrorType e) {
            std::cerr << "Error starting stream: " << e << std::endl;
        }
    }
}

void AudioEngine::stop() {
    if (m_audio && m_isRunning) {
        try {
            m_audio->stopStream();
            m_isRunning = false;
            std::cout << "Audio stream stopped" << std::endl;
        } catch (RtAudioErrorType e) {
            std::cerr << "Error stopping stream: " << e << std::endl;
        }
    }

    if (m_audio && m_audio->isStreamOpen()) {
        m_audio->closeStream();
    }
}

int AudioEngine::audioCallback(void* outputBuffer, void* inputBuffer,
                               unsigned int nFrames, double /*streamTime*/,
                               RtAudioStreamStatus status, void* userData) {
    AudioEngine* engine = static_cast<AudioEngine*>(userData);

    float* output = static_cast<float*>(outputBuffer);
    float* input = static_cast<float*>(inputBuffer);

    if (status) {
        std::cerr << "Stream underflow or overflow detected!" << std::endl;
    }

    engine->processAudio(output, input, nFrames);

    return 0;
}

void AudioEngine::processAudio(float* output, float* /*input*/, unsigned int nFrames) {
    // no dsp here. idrk what to add honestly.
    for (unsigned int i = 0; i < nFrames * 2; i++) {
        output[i] = 0.0f;
    }
}
