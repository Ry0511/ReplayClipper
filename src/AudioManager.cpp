//
// Date       : 09/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "AudioManager.h"

#include "rtaudio/RtAudio.h"

#include <iostream>
#include <deque>
#include <cassert>
#include <mutex>

namespace ReplayClipper {

    static bool gHasInitialised = false;
    static RtAudio gAudio{
            RtAudio::UNSPECIFIED,
            [](RtAudioErrorType err, const std::string& msg) {
                std::cout << "[RtError] ~ " << (int) err << ", '" << msg << "'" << std::endl;
            }
    };

    static bool gIsPaused = true;
    static unsigned int gBufferFrames = 512;

    class ByteStream {

      private:
        uint8_t* m_Data;
        size_t m_Length;
        size_t m_StreamPos;

      public:
        ByteStream(uint8_t* data, size_t length)
                : m_Data(data),
                  m_Length(length),
                  m_StreamPos(0) {

        }

        template<class T>
        ByteStream(T* data, size_t count)
                : m_Data(reinterpret_cast<uint8_t*>(data)),
                  m_Length(count * sizeof(T)),
                  m_StreamPos(0) {

        }

      public:
        inline bool IsValid() const noexcept {
            return m_Data != nullptr && m_Length > 0;
        }

      public:
        void Reset() noexcept {
            m_StreamPos = 0;
        }

        bool Seek(size_t pos) {
            if (pos <= m_Length) {
                m_StreamPos = pos;
                return true;
            }
            return false;
        }

      public:
        inline size_t Fetch(uint8_t* out, size_t count) {

            // Pedantic
            assert(m_Length >= m_StreamPos);
            assert(m_Data != nullptr);
            assert(out != nullptr);

            const size_t remaining_bytes = m_Length - m_StreamPos;
            const size_t bytes_to_write = std::min(remaining_bytes, count);

            // Read Bytes into 'out'
            if (bytes_to_write > 0) {
                std::memcpy(out, m_Data + m_StreamPos, bytes_to_write);
                m_StreamPos += bytes_to_write;
            }

            return bytes_to_write;
        }
    };

    std::mutex gAudioQueueMutex{};
    std::deque<ByteStream> gAudioQueue{};
    ByteStream gCurrentStream{nullptr, 0};

    static int InternalAudioCallback(
            void* output_buffer,
            void*,
            unsigned int frame_count,
            double stream_time,
            RtAudioStreamStatus status,
            void*
    ) {

        if (status) {
            std::cerr << "Stream underflow detected!" << std::endl;
        }

        float* data = reinterpret_cast<float*>(output_buffer);
        size_t requested_bytes = frame_count * 2 * sizeof(float);

        std::memset(data, 0, requested_bytes);

        // Stream Is Paused
        if (gIsPaused) {
            return 0;
        }

        if (!gCurrentStream.IsValid() && !gAudioQueue.empty()) {
            std::unique_lock guard{gAudioQueueMutex};
            gCurrentStream = gAudioQueue.front();
            gAudioQueue.pop_front();
        }

        size_t written = gCurrentStream.Fetch(reinterpret_cast<uint8_t*>(data), requested_bytes);

        if (written < requested_bytes && !gAudioQueue.empty()) {
            {
                std::unique_lock guard{gAudioQueueMutex};
                gCurrentStream = gAudioQueue.front();
                gAudioQueue.pop_front();
            }
            gCurrentStream.Fetch(reinterpret_cast<uint8_t*>(data), requested_bytes);
        }

        for (size_t i = 0; i < requested_bytes / sizeof(float); ++i) {
            data[i] *= 0.05F;
        }

        return 0;
    }

    bool AudioManager::Initialise() {

        try {

            assert(gAudio.getDefaultOutputDevice() != 0);
            RtAudio::DeviceInfo info = gAudio.getDeviceInfo(gAudio.getDefaultOutputDevice());
            std::cout << "Default Device ~ '" << info.name << "', " << info.ID << std::endl;

            RtAudio::StreamParameters params{};
            params.deviceId = gAudio.getDefaultOutputDevice();
            params.firstChannel = 0;
            params.nChannels = 2;

            RtAudio::StreamOptions options{};
            options.flags = RTAUDIO_MINIMIZE_LATENCY;

            gAudio.openStream(&params, nullptr, RTAUDIO_FLOAT32, 48000, &gBufferFrames, &InternalAudioCallback, nullptr, &options);
            assert(params.nChannels == 2);
            gAudio.startStream();
            gHasInitialised = true;
            return true;

        } catch (const std::exception& err) {
            std::cout << "RtAudio Exception ~ " << err.what() << std::endl;
            gHasInitialised = false;
            return false;
        }
    }

    void AudioManager::Terminate() {
        if (gAudio.isStreamOpen()) {
            gAudio.stopStream();
        }
    }

    void AudioManager::Pause() {
        gIsPaused = true;
    }

    void AudioManager::Resume() {
        gIsPaused = false;
    }

    void AudioManager::EnqueueOnce(uint8_t* data, size_t length) {
        std::unique_lock guard{gAudioQueueMutex};
        gAudioQueue.emplace_back(data, length);
    }

} // ReplayClipper