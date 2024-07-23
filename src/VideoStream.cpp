//
// Date       : 12/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "VideoStream.h"

#include "Logging.h"
#include "Stopwatch.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

// Audio Resampling
#include <libswresample/swresample.h>

// Image Rescaling
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

}

#include <deque>

namespace ReplayClipper {

    //############################################################################//
    // | Lazy Video Stream |
    //############################################################################//

    class LazyVideoStream {

      private: // @off
        AVFormatContext* m_FormatContext;
        AVPacket*        m_Packet;
        AVCodecContext*  m_VideoCodec;
        int              m_VideoIndex;
        AVCodecContext*  m_AudioCodec;
        int              m_AudioIndex;
      // @on

      public:
        LazyVideoStream() : m_FormatContext(nullptr),
                            m_Packet(nullptr),
                            m_VideoCodec(nullptr),
                            m_VideoIndex(-1),
                            m_AudioCodec(nullptr),
                            m_AudioIndex(-1) {
        }

        ~LazyVideoStream() {
            REPLAY_TRACE("Destroying Video Stream");
            avformat_close_input(&m_FormatContext);
            av_packet_free(&m_Packet);
            avcodec_free_context(&m_VideoCodec);
            avcodec_free_context(&m_AudioCodec);
        }

      private:
        void Reset() {

            REPLAY_TRACE("VideoStream Reset()");
            // Delete
            avformat_close_input(&m_FormatContext);
            av_packet_free(&m_Packet);
            avcodec_free_context(&m_VideoCodec);
            m_VideoIndex = -1;
            avcodec_free_context(&m_AudioCodec);
            m_AudioIndex = -1;

            // Alloc
            m_FormatContext = avformat_alloc_context();
            m_Packet = av_packet_alloc();

            REPLAY_ASSERT(m_FormatContext && m_Packet);

        }

      public:
        bool OpenStream(const fs::path& path) {
            Reset();

            std::string path_str = path.generic_string();

            if (avformat_open_input(&m_FormatContext, path_str.c_str(), nullptr, nullptr) < 0) {
                REPLAY_TRACE("Failed to open input '{}'", path_str);
                return false;
            }

            if (avformat_find_stream_info(m_FormatContext, nullptr) < 0) {
                REPLAY_TRACE("Failed to get stream info for '{}'", path_str);
                return false;
            }

            for (int i = 0; i < m_FormatContext->nb_streams; ++i) {
                AVCodecParameters* params = m_FormatContext->streams[i]->codecpar;
                const AVCodec* codec = avcodec_find_decoder(params->codec_id);

                AVCodecContext** context = nullptr;

                // Video Stream
                if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
                    m_VideoIndex = i;
                    context = &m_VideoCodec;

                    // Audio Stream
                } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
                    m_AudioIndex = i;
                    context = &m_AudioCodec;
                }

                // Skip
                if (context == nullptr) {
                    continue;
                }

                // Unknown Codec
                if (codec == nullptr) {
                    REPLAY_WARN("Unsupported Codec for file '{}'", path_str);
                    return false;
                }

                *context = avcodec_alloc_context3(codec);
                REPLAY_ASSERT(context);

                if (avcodec_parameters_to_context(*context, params) < 0) {
                    REPLAY_TRACE("Failed to copy codec parameters to codec context");
                    return false;
                }

                if (avcodec_open2(*context, codec, nullptr) < 0) {
                    REPLAY_TRACE("Failed to open codec '{}'", codec->long_name);
                    return false;
                }
            }

            return true;
        }

      public:
        AVFormatContext* GetFormatContext() const noexcept {
            return m_FormatContext;
        }

        AVCodecContext* GetVideoContext() const noexcept {
            return m_VideoCodec;
        }

        AVCodecContext* GetAudioContext() const noexcept {
            return m_AudioCodec;
        }

      public:
        bool IsOpen() const noexcept {
            return m_FormatContext && m_FormatContext->pb && !m_FormatContext->pb->eof_reached;
        }

      public:
        static constexpr int STREAM_FLAG_AUDIO = 1 << 0;
        static constexpr int STREAM_FLAG_VIDEO = 1 << 1;
        static constexpr int STREAM_FLAG_BOTH = STREAM_FLAG_AUDIO | STREAM_FLAG_VIDEO;

      public:
        bool NextFrame(AVFrame* out, int stream_flags = STREAM_FLAG_BOTH) {

            if (!IsOpen()) {
                return false;
            }

            while (av_read_frame(m_FormatContext, m_Packet) >= 0) {

                AVCodecContext* codec_context = nullptr;

                // Figure out what stream the packet is
                if (m_Packet->stream_index == m_VideoIndex && (stream_flags & STREAM_FLAG_VIDEO)) {
                    codec_context = m_VideoCodec;

                } else if (m_Packet->stream_index == m_AudioIndex && (stream_flags & STREAM_FLAG_AUDIO)) {
                    codec_context = m_AudioCodec;
                }

                // Skip Packet
                if (codec_context == nullptr) {
                    av_packet_unref(m_Packet);
                    continue;
                }

                int res = avcodec_send_packet(codec_context, m_Packet);

                while (res >= 0) {
                    res = avcodec_receive_frame(codec_context, out);
                    // EAGAIN means we need more packets to get a full frame
                    // EOF just means that there is no more frames and or packets
                    if (res == AVERROR(EAGAIN)) {
                        break;
                    } else if (res == AVERROR_EOF || res < 0) {
                        av_packet_unref(m_Packet);
                        REPLAY_WARN("Error Getting Frame: {}", res);
                        return false;
                    }

                    if (res >= 0) {
                        out->time_base = m_FormatContext->streams[m_Packet->stream_index]->time_base;
                        av_packet_unref(m_Packet);
                        return true;
                    }
                }

                av_packet_unref(m_Packet);
            }

            return false;
        }

      public:
        bool Seek(double ts) noexcept {
            if (!IsOpen()) {
                return false;
            }

            int res = av_seek_frame(GetFormatContext(), -1, ts * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
            if (res < 0) {
                return false;
            }

            avcodec_flush_buffers(m_VideoCodec);
            avcodec_flush_buffers(m_AudioCodec);

            // Walk forward until we get to the desired timestamp
            Stopwatch watch{};
            watch.Start();
            int64_t time = int64_t(ts * AV_TIME_BASE);
            int64_t frame_time_rescaled = 0;
            size_t frames_skipped = 0;
            AVFrame* temp = av_frame_alloc();
            bool got_frame = false;
            do {
                got_frame = NextFrame(temp, STREAM_FLAG_VIDEO);
                constexpr int64_t OFFSET = AV_TIME_BASE / 2;
                frame_time_rescaled = av_rescale_q(temp->pts, temp->time_base, AV_TIME_BASE_Q) + OFFSET;
                ++frames_skipped;
            } while (got_frame && time > frame_time_rescaled);
            watch.End();

            REPLAY_TRACE(
                    "Forward seek skipped {} frames from {} to {}; Total Seek Time {:.2f}ms",
                    frames_skipped,
                    time,
                    frame_time_rescaled,
                    watch.Millis<float>()
            );
            av_frame_free(&temp);


            return true;
        }

      public:
        unsigned int GetChannels() const noexcept {
            if (!m_AudioCodec) {
                return 0;
            }
            return m_AudioCodec->ch_layout.nb_channels;
        }

        unsigned int GetSampleRate() const noexcept {
            if (!m_AudioCodec) {
                return 0;
            }
            return m_AudioCodec->sample_rate;
        }

      public:
        int64_t GetDuration() const noexcept {
            if (!m_FormatContext) {
                return 0LL;
            }
            return m_FormatContext->duration;
        }
    };

    static bool ConvertToRGB(
            AVFrame* src_frame,
            Frame::video_frame_t& out,
            int desired_width = -1,
            int desired_height = -1
    ) {

        // Source Params
        int src_w = src_frame->width;
        int src_h = src_frame->height;
        REPLAY_ASSERT(src_w > 0 && src_h > 0);
        const AVPixelFormat src_fmt = static_cast<const AVPixelFormat>(src_frame->format);

        int width = (desired_width > 0) ? desired_width : src_w;
        int height = (desired_height > 0) ? desired_height : src_h;

        // Rescaling Context
        SwsContext* sws_ctx = sws_getCachedContext(
                nullptr,
                // Input Options
                src_w, src_h, src_fmt,
                // Output Options
                width, height, AV_PIX_FMT_RGB24,
                // Scaling
                SWS_BILINEAR,
                nullptr,
                nullptr,
                nullptr
        );

        if (!sws_ctx) {
            REPLAY_WARN("Failed to get SwsContext");
            sws_freeContext(sws_ctx);
            return false;
        }

        // Setup Output Buffer
        size_t dest_size = size_t(width) * size_t(height) * 3ULL;
        Frame::video_frame_t& video = out;
        video.Width = width;
        video.Height = height;
        video.Timestamp = av_rescale_q(src_frame->pts, src_frame->time_base, AV_TIME_BASE_Q);
        video.Pixels.resize(dest_size, 255);

        // Expected Input; We only care for the first of each
        const int ls[4]{3 * width};
        uint8_t* data[4]{video.Pixels.data()};

        sws_scale(
                sws_ctx,
                src_frame->data,
                src_frame->linesize,
                0,
                src_frame->height,
                data,
                ls
        );

        sws_freeContext(sws_ctx);
        return true;

    }

    static bool ConvertToPackedAudio(
            AVFrame* frame,
            Frame::audio_frame_t& audio
    ) {

        REPLAY_ASSERT(frame->nb_samples > 0 && frame->ch_layout.nb_channels > 0);

        SwrContext* ctx = nullptr;
        int res = swr_alloc_set_opts2(
                &ctx,
                // Out
                &frame->ch_layout,
                static_cast<AVSampleFormat>(AV_SAMPLE_FMT_FLT),
                frame->sample_rate,
                // In
                &frame->ch_layout,
                static_cast<AVSampleFormat>(frame->format),
                frame->sample_rate,
                // Logging
                0,
                nullptr
        );

        if (res != 0) {
            REPLAY_WARN("Failed to allocate SwrContext");
            return false;
        }

        res = swr_init(ctx);
        if (res < 0) {
            REPLAY_WARN("Failed to initialise SwrContext");
            swr_free(&ctx);
            return false;
        }

        // Resize Vector
        audio.Samples.resize(size_t(frame->nb_samples) * size_t(frame->ch_layout.nb_channels) * sizeof(float));

        uint8_t* out[1]{audio.Samples.data()};

        res = swr_convert(
                ctx,
                out,
                frame->nb_samples,
                const_cast<const uint8_t**>(frame->data),
                frame->nb_samples
        );

        if (res < 0) {
            REPLAY_WARN("Error converting audio; {}", res);
            swr_free(&ctx);
            return false;
        }

        // Setting output
        audio.SampleRate = frame->sample_rate;
        audio.Channels = frame->ch_layout.nb_channels;
        audio.Timestamp = av_rescale_q(frame->pts, frame->time_base, AV_TIME_BASE_Q);

        swr_free(&ctx);
        return true;
    }

    //############################################################################//
    // | Impl |
    //############################################################################//

    class FramePool {

      private:
        int m_PoolSize;
        std::deque<Frame> m_Pool;

      public:
        FramePool(int size) : m_PoolSize(size), m_Pool() {

        }

        ~FramePool() = default;

      public:
        bool HasSpace() const noexcept {
            return m_Pool.size() <= m_PoolSize;
        }

        bool HasFrames() const noexcept {
            return !m_Pool.empty();
        }

      public:
    };

    class VideoStream::Impl {

      private:
        LazyVideoStream m_Stream;
        FramePool m_FramePool;

      public:
        Impl(int pool_size)
                : m_Stream(),
                  m_FramePool(pool_size) {

        }

      public:
        bool OpenStream(const fs::path& path) {
            return m_Stream.OpenStream(path);
        }

      public:
        Frame NextFrame() noexcept {
            AVFrame* frame = av_frame_alloc();
            REPLAY_ASSERT(frame);
            m_Stream.NextFrame(frame);

            // Video
            if (frame->width > 0 && frame->height > 0) {
                Frame::video_frame_t video{};
                Frame out{std::move(video)};
                ConvertToRGB(frame, out);
                av_frame_free(&frame);
                return out;

                // Audio
            } else if (frame->sample_rate > 0 && frame->nb_samples > 0) {
                Frame::audio_frame_t audio{};
                ConvertToPackedAudio(frame, audio);
                av_frame_free(&frame);
                return Frame{std::move(audio)};
            }

            // null frame
            av_frame_free(&frame);
            return Frame{};
        }

      public:
        bool IsOpen() const noexcept {
            return m_Stream.IsOpen();
        }

      public:
        inline bool Seek(double ts) noexcept {
            return m_Stream.Seek(ts);
        }

      public:
        unsigned int GetChannels() const noexcept {
            return m_Stream.GetChannels();
        }

        unsigned int GetSampleRate() const noexcept {
            return m_Stream.GetSampleRate();
        }

      public:
        int64_t GetDuration() const noexcept {
            return m_Stream.GetDuration();
        }
    };

    //############################################################################//
    // | Main Class |
    //############################################################################//

    VideoStream::VideoStream(int pool_size) : m_Impl(std::make_unique<Impl>(pool_size)) {

    }

    VideoStream::~VideoStream() {

    }

    VideoStream::VideoStream(VideoStream&& other) : m_Impl(std::move(other.m_Impl)) {

    }

    VideoStream& VideoStream::operator =(VideoStream&& other) {
        m_Impl = std::exchange(other.m_Impl, nullptr);
        return *this;
    }

    bool VideoStream::IsOpen() const noexcept {
        return m_Impl->IsOpen();
    }

    bool VideoStream::OpenStream(const fs::path& file) {
        return m_Impl->OpenStream(file);
    }

    Frame VideoStream::NextFrame() noexcept {
        return m_Impl->NextFrame();
    }

    bool VideoStream::Seek(double seconds) noexcept {
        return m_Impl->Seek(seconds);
    }

    unsigned int VideoStream::GetSampleRate() const noexcept {
        return m_Impl->GetSampleRate();
    }

    unsigned int VideoStream::GetChannels() const noexcept {
        return m_Impl->GetChannels();
    }

    int64_t VideoStream::GetDuration() const noexcept {
        return m_Impl->GetDuration();
    }

} // ReplayClipper
