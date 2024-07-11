//
// Date       : 04/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "VideoFile.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <format>
#include <string>
#include <cassert>
#include <iostream>
#include <deque>

#define REPLAY_TRACE(src, ...) std::cout << "[VIDEO_FILE] ~ " << std::format(src, __VA_ARGS__) << std::endl

namespace ReplayClipper {

    //############################################################################//
    // | FFMPEG HELPERS |
    //############################################################################//

    struct ffmpeg_stream_t {
        AVCodecContext* CodecContext;
        int StreamIndex;

        ffmpeg_stream_t() : CodecContext(nullptr) {
        }

        inline bool IsValid() const noexcept {
            return CodecContext && StreamIndex >= 0;
        }
    };

    struct ffmpeg_context_t {

      public:
        constexpr static size_t FRAME_POOL_SIZE = 4;

      public:
        AVFormatContext* FormatContext;
        AVPacket* Packet;
        AVFrame* Frame;
        int CurrentFrame;
        ffmpeg_stream_t AudioStream;
        ffmpeg_stream_t VideoStream;

      public:
        ffmpeg_context_t()
                : FormatContext(nullptr),
                  Packet(av_packet_alloc()),
                  Frame(av_frame_alloc()),
                  AudioStream(),
                  VideoStream() {
        }

      public:
        inline bool IsValid() const noexcept {
            return FormatContext
                   && Packet
                   && AudioStream.IsValid()
                   && VideoStream.IsValid();
        }
    };

    //############################################################################//
    // | VIDEO FILE FRAME |
    //############################################################################//

    int VideoFile::Frame::Width() {
        assert(IsVideoFrame());
        return m_Frame->width;
    }

    int VideoFile::Frame::Height() {
        assert(IsVideoFrame());
        return m_Frame->height;
    }

    void VideoFile::Frame::CopyInto(std::vector<Pixel>& pixels) {
        int width = Width();
        int height = Height();
        assert(width > 0 && height > 0);
        pixels.resize(width * height);
        std::memset(pixels.data(), 255, pixels.size() * sizeof(decltype(pixels[0])));

        AVPixelFormat format = static_cast<AVPixelFormat>(m_Frame->format);

        if (m_Frame->format != AV_PIX_FMT_YUV420P) {
            return;
        }

        uint8_t** data = m_Frame->data;
        int* lines = m_Frame->linesize;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const uint8_t Y = data[0][lines[0] * y + x];
                const uint8_t U = data[1][lines[1] * (y >> 1) + (x >> 1)];
                const uint8_t V = data[2][lines[2] * (y >> 1) + (x >> 1)];

                int C = Y - 16;
                int D = U - 128;
                int E = V - 128;

                uint8_t r = std::clamp(((298 * C + 409 * E + 128) >> 8), 0, 255);
                uint8_t g = std::clamp(((298 * C - 100 * D - 208 * E + 128) >> 8), 0, 255);
                uint8_t b = std::clamp(((298 * C + 516 * D + 128) >> 8), 0, 255);

                pixels[y * width + x] = Pixel{r, g, b};
            }
        }
    }

    void VideoFile::Frame::CopyInto(std::vector<uint8_t>& audio) {
        assert(IsAudioFrame());
        assert(m_Frame->format == AV_SAMPLE_FMT_FLTP);
        assert(m_Frame != nullptr);

        // Determine number of bytes per sample
        int bytes_per_sample = av_get_bytes_per_sample(static_cast<AVSampleFormat>(m_Frame->format));
        if (bytes_per_sample < 0) {
            std::cerr << "Unsupported sample format" << std::endl;
            return;
        }

        // Prepare the destination vector with the correct size
        size_t total_bytes = m_Frame->nb_samples * m_Frame->ch_layout.nb_channels * bytes_per_sample;
        audio.resize(total_bytes);

        if (av_sample_fmt_is_planar(static_cast<AVSampleFormat>(m_Frame->format)) == 1) {
            for (int sample_index = 0; sample_index < m_Frame->nb_samples; ++sample_index) {
                for (int channel_index = 0; channel_index < m_Frame->ch_layout.nb_channels; ++channel_index) {
                    uint8_t* src = m_Frame->data[channel_index] + sample_index * bytes_per_sample;
                    uint8_t* dest = audio.data() + (sample_index * m_Frame->ch_layout.nb_channels + channel_index) * bytes_per_sample;
                    std::memcpy(dest, src, bytes_per_sample);
                }
            }
        } else {
            std::memcpy(audio.data(), m_Frame->data[0], total_bytes);
        }
    }

    //############################################################################//
    // | VIDEO FILE |
    //############################################################################//

    class VideoFile::Impl {

      private:
        ffmpeg_context_t m_Context;

      public:
        Impl() : m_Context() {
        }

        ~Impl() {
            // Dealloc Video Stream
            ffmpeg_stream_t& video = m_Context.VideoStream;
            avcodec_free_context(&video.CodecContext);
            video.StreamIndex = -1;

            // Dealloc Audio Stream
            ffmpeg_stream_t& audio = m_Context.AudioStream;
            avcodec_free_context(&audio.CodecContext);
            audio.StreamIndex = -1;

            // Dealloc Context
            av_packet_free(&m_Context.Packet);
            av_frame_free(&m_Context.Frame);
            avformat_close_input(&m_Context.FormatContext);

        }

      public:
        double GetDurationInSeconds() const noexcept {
            assert(m_Context.FormatContext);
            double seconds = m_Context.FormatContext->duration / double{AV_TIME_BASE};
            return seconds;
        }

        bool SeekToSeconds(double seconds) const noexcept {
            assert(m_Context.FormatContext);
            int64_t time = AV_TIME_BASE * seconds;

            const ffmpeg_stream_t& audio_stream = m_Context.AudioStream;
            const ffmpeg_stream_t& video_stream = m_Context.VideoStream;

            int64_t video_time = av_rescale_q(time, AV_TIME_BASE_Q, video_stream.CodecContext->time_base);
            int64_t audio_time = av_rescale_q(time, AV_TIME_BASE_Q, audio_stream.CodecContext->time_base);
            int vid_seek_res = av_seek_frame(m_Context.FormatContext, video_stream.StreamIndex, video_time, AVSEEK_FLAG_BACKWARD);
            int aud_seek_res = av_seek_frame(m_Context.FormatContext, audio_stream.StreamIndex, audio_time, AVSEEK_FLAG_BACKWARD);

            return vid_seek_res >= 0 && aud_seek_res >= 0;
        }

        std::string GetVideoStreamCodecLongName() const noexcept {
            return m_Context.VideoStream.CodecContext->codec->long_name;
        }

        std::string GetAudioStreamCodecLongName() const noexcept {
            return m_Context.VideoStream.CodecContext->codec->long_name;
        }

      public:
        bool OpenFile(const fs::path& path) {

            // Close
            if (m_Context.FormatContext != nullptr) {
                avformat_close_input(&m_Context.FormatContext);
                avcodec_free_context(&m_Context.AudioStream.CodecContext);
                avcodec_free_context(&m_Context.VideoStream.CodecContext);
                assert(m_Context.FormatContext == nullptr
                       && m_Context.AudioStream.CodecContext == nullptr
                       && m_Context.VideoStream.CodecContext == nullptr);
            }

            // Open File
            auto file_path = path.generic_string();
            if (avformat_open_input(&m_Context.FormatContext, file_path.c_str(), nullptr, nullptr) != 0) {
                REPLAY_TRACE("Failed to open input '{}'", file_path);
                return false;
            }

            // Process Stream
            if (avformat_find_stream_info(m_Context.FormatContext, nullptr) < 0) {
                REPLAY_TRACE("Failed to process stream info");
                return false;
            }

            bool has_found_video_stream = false;
            bool has_found_audio_stream = false;

            for (int i = 0; i < m_Context.FormatContext->nb_streams; ++i) {
                AVCodecParameters* params = m_Context.FormatContext->streams[i]->codecpar;
                const AVCodec* codec = avcodec_find_decoder(params->codec_id);

                ffmpeg_stream_t* current_stream = nullptr;

                if (!has_found_video_stream && params->codec_type == AVMEDIA_TYPE_VIDEO) {
                    current_stream = &m_Context.VideoStream;
                    has_found_video_stream = true;

                } else if (!has_found_audio_stream && params->codec_type == AVMEDIA_TYPE_AUDIO) {
                    current_stream = &m_Context.AudioStream;
                    has_found_audio_stream = true;
                }

                // Could a different type of stream
                if (current_stream == nullptr) {
                    continue;
                }

                current_stream->StreamIndex = i;
                current_stream->CodecContext = avcodec_alloc_context3(codec);

                if (avcodec_parameters_to_context(current_stream->CodecContext, params) < 0) {
                    REPLAY_TRACE("Failed to copy codec parameters to codec context");
                    return false;
                }

                if (avcodec_open2(current_stream->CodecContext, codec, nullptr) < 0) {
                    REPLAY_TRACE("Failed to open codec {}", codec->long_name);
                    return false;
                }

            }

            return true;
        }

      public:
        VideoFile::Frame NextFrame() {

            AVFormatContext* fmt_ctx = m_Context.FormatContext;
            AVPacket* packet = m_Context.Packet;
            AVFrame* frame = m_Context.Frame;
            int stream_type = VideoFile::Frame::FRAME_TYPE_UNKNOWN;

            ffmpeg_stream_t& video_stream = m_Context.VideoStream;
            ffmpeg_stream_t& audio_stream = m_Context.AudioStream;

            while (av_read_frame(fmt_ctx, packet) >= 0) {

                ffmpeg_stream_t* active_stream = nullptr;

                // Is Video Packet
                if (packet->stream_index == video_stream.StreamIndex) {
                    active_stream = &video_stream;
                    stream_type = VideoFile::Frame::FRAME_TYPE_VIDEO;

                    // Is Audio Packet
                } else if (packet->stream_index == audio_stream.StreamIndex) {
                    active_stream = &audio_stream;
                    stream_type = VideoFile::Frame::FRAME_TYPE_AUDIO;
                }

                // Skip Packet
                if (active_stream == nullptr) {
                    av_packet_unref(packet);
                    continue;
                }

                int res = avcodec_send_packet(active_stream->CodecContext, packet);

                while (res >= 0) {
                    res = avcodec_receive_frame(active_stream->CodecContext, frame);

                    // EAGAIN means we need more packets to get a full frame
                    // EOF just means that there is no more frames and or packets
                    if (res == AVERROR(EAGAIN)) {
                        break;
                    } else if (res == AVERROR_EOF) {
                        return Frame{nullptr, VideoFile::Frame::FRAME_TYPE_EOF};
                    } else if (res < 0) {
                        REPLAY_TRACE("Failure in receiving frame");
                        return Frame{nullptr, VideoFile::Frame::FRAME_TYPE_UNKNOWN};
                    }

                    if (res >= 0) {
                        av_packet_unref(packet);
                        return Frame{frame, stream_type};
                    }
                }

                av_packet_unref(packet);
            }
            return Frame{nullptr, VideoFile::Frame::FRAME_TYPE_UNKNOWN};
        }
    };

    //############################################################################//
    // | VIDEO FILE INTERFACE |
    //############################################################################//

    VideoFile::VideoFile() : m_Impl(std::make_unique<Impl>()) {

    }

    VideoFile::~VideoFile() {

    }

    bool VideoFile::OpenFile(const fs::path& path) {
        return m_Impl->OpenFile(path);
    }

    bool VideoFile::Seek(double seconds) {
        return m_Impl->SeekToSeconds(seconds);
    }

    VideoFile::Frame VideoFile::NextFrame() {
        return m_Impl->NextFrame();
    }

} // ReplayClipper