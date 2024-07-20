//
// Date       : 09/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_AUDIOPLAYER_H
#define REPLAYCLIPPER_AUDIOPLAYER_H

#include "rtaudio/RtAudio.h"

#include <deque>
#include <mutex>
#include <cassert>

namespace ReplayClipper {

    class ByteStream {
      private:
        std::vector<uint8_t> m_Data;
        size_t m_Position;

      public:
        ByteStream() : m_Data(), m_Position(0) {}
        ByteStream(std::vector<uint8_t>&& data) : m_Data(std::move(data)), m_Position(0) {}

      public:
        inline size_t Length() const noexcept {
            return m_Data.size();
        }
        inline size_t Remaining() const noexcept {
            assert(m_Position <= Length());
            return Length() - m_Position;
        }
        inline const uint8_t* Data() const noexcept {
            return m_Data.data();
        }

      public:
        inline void Reset() noexcept {
            m_Position = 0;
        }
        inline size_t Fetch(uint8_t* out, size_t count) noexcept {
            assert(out != nullptr);
            assert(Length() >= m_Position);

            const size_t remaining_bytes = Length() - m_Position;
            const size_t bytes_to_write = (remaining_bytes <= count) ? remaining_bytes : count;

            // Read Bytes into 'out'
            if (bytes_to_write > 0) {
                std::memcpy(out, Data() + m_Position, bytes_to_write);
                m_Position += bytes_to_write;
            }

            return bytes_to_write;
        }
    };

    class AudioPlayer {

      private:
        RtAudio m_Handle;
        std::atomic_int m_Index;
        std::deque<ByteStream> m_AudioQueues[2];
        int m_Channels;
        float m_AudioScalar;

      private:
        bool m_IsPaused = false;

      public:
        AudioPlayer();
        ~AudioPlayer();

      public:
        void SetVolumeScale(float scale) {
            m_AudioScalar = scale;
        }

      public:
        bool IsStreamOpen() {
            return m_Handle.isStreamOpen();
        }
        bool OpenStream(int channels, int sample_rate);

      public:
        bool CloseStream();

      public:
        void Play() noexcept;
        void Pause() noexcept;
        void Resume() noexcept;

      public:
        void EnqueueOnce(std::vector<uint8_t>&& samples);

      private:
        static int AudioCallback(void*, void*, unsigned int, double, RtAudioStreamStatus, void*);
    };

} // ReplayClipper

#endif
