//
// Date       : 04/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_VIDEOFILE_H
#define REPLAYCLIPPER_VIDEOFILE_H

#include <filesystem>
#include <memory>

extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
};

namespace ReplayClipper {

    namespace fs = std::filesystem;

    class VideoFile {

      private:
        struct ffmpeg_context_t {
            AVFormatContext* FormatContext = nullptr;
        };

        struct ffmpeg_stream_t {
            AVCodecContext* CodecContext = nullptr;
            int StreamIndex = -1;
        };

        struct ffmpeg_packet_t {
            AVPacket* Packet;
            AVFrame* Frame;
        };

      private:
        std::unique_ptr<ffmpeg_context_t> m_Context;
        std::unique_ptr<ffmpeg_stream_t> m_VideoStream;
        std::unique_ptr<ffmpeg_packet_t> m_Stream;

      public:
        VideoFile();
        ~VideoFile();

      public:
        bool OpenFile(const fs::path& path);

      public:
        void Seek(double seconds);
        AVFrame* NextFrame();
    };

} // ReplayClipper

#endif
