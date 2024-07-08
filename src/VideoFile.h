//
// Date       : 04/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_VIDEOFILE_H
#define REPLAYCLIPPER_VIDEOFILE_H

#include <filesystem>
#include <memory>

struct AVFrame;

namespace ReplayClipper {

    namespace fs = std::filesystem;

    struct Pixel {
        uint8_t Red, Green, Blue;
    };

    class VideoFile {

      public:
        class Frame {

          public:
            constexpr static int FRAME_TYPE_UNKNOWN = 0;
            constexpr static int FRAME_TYPE_EOF = 1;
            constexpr static int FRAME_TYPE_VIDEO = 2;
            constexpr static int FRAME_TYPE_AUDIO = 3;

          private:
            AVFrame* m_Frame;
            int m_FrameType;

          public:
            Frame(AVFrame* frame, int frame_type) : m_Frame(frame), m_FrameType(frame_type) {

            }

          public:
            inline bool IsValid() const noexcept {
                return m_Frame != nullptr;
            }
            inline bool IsVideoFrame() {
                return m_Frame != nullptr && m_FrameType == FRAME_TYPE_VIDEO;
            }
            inline bool IsAudioFrame() {
                return m_Frame != nullptr && m_FrameType == FRAME_TYPE_AUDIO;
            }
            inline bool IsEOF() const noexcept {
                return m_FrameType == FRAME_TYPE_EOF;
            }

          public:
            int Width();
            int Height();
            void CopyInto(std::vector<Pixel>& pixels);
        };

      private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;

      public:
        VideoFile();
        ~VideoFile();

      public:
        bool OpenFile(const fs::path& path);

      public:
        bool Seek(double seconds);
        Frame NextFrame();
    };

} // ReplayClipper

#endif
