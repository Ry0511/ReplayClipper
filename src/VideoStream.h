//
// Date       : 12/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_VIDEOSTREAM_H
#define REPLAYCLIPPER_VIDEOSTREAM_H

#include <memory>
#include <filesystem>
#include <variant>

namespace ReplayClipper {

    namespace fs = std::filesystem;

    class Frame {
      public:
        struct video_frame_t {
            int Width, Height;
            int64_t Timestamp;
            std::vector<uint8_t> Pixels;
        };
        struct audio_frame_t {
            int Channels, SampleRate;
            int64_t Timestamp;
            std::vector<uint8_t> Samples;
        };

      private:
        std::variant<video_frame_t, audio_frame_t, nullptr_t> m_Data;

      public:
        explicit Frame() : m_Data(nullptr) {}
        explicit Frame(audio_frame_t&& audio) : m_Data(std::move(audio)) {}
        explicit Frame(video_frame_t&& video) : m_Data(std::move(video)) {}

      public:
        bool IsValid() const noexcept {
            return !std::holds_alternative<nullptr_t>(m_Data);
        }
        bool IsVideo() const noexcept {
            return std::holds_alternative<video_frame_t>(m_Data);
        }
        bool IsAudio() const noexcept {
            return std::holds_alternative<audio_frame_t>(m_Data);
        }

      public:
        operator const video_frame_t&() const noexcept {
            return std::get<video_frame_t>(m_Data);
        }
        operator video_frame_t&() noexcept {
            return std::get<video_frame_t>(m_Data);
        }

      public:
        operator const audio_frame_t&() const noexcept {
            return std::get<audio_frame_t>(m_Data);
        }
        operator audio_frame_t&() noexcept {
            return std::get<audio_frame_t>(m_Data);
        }

      public:
        int64_t Timestamp() const noexcept {
            return std::visit<int64_t>(
                    [](auto&& it) -> int64_t {
                        using T = std::decay_t<decltype(it)>;
                        if constexpr (std::is_same_v<T, audio_frame_t>
                                      || std::is_same_v<T, video_frame_t>) {
                            return it.Timestamp;
                        } else {
                            return int64_t{-1};
                        }
                    }, m_Data
            );
        }

    };

    class VideoStream {

      private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;

      public:
        VideoStream(int pool_size = 5);
        ~VideoStream();

      public:
        VideoStream(const VideoStream&) = delete;
        VideoStream& operator =(const VideoStream&) = delete;
        VideoStream(VideoStream&&);
        VideoStream& operator =(VideoStream&&);

      public:
        bool IsOpen() const noexcept;
        bool OpenStream(const fs::path& file);

      public:
        Frame NextFrame() noexcept;

      public:
        bool Seek(double seconds) noexcept;

      public:
        unsigned int GetChannels() const noexcept;
        unsigned int GetSampleRate() const noexcept;
    };

} // ReplayClipper

#endif
