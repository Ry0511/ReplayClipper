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
}

#include <format>
#include <string>
#include <cassert>
#include <iostream>

#define VF_LOG(src, ...) std::cout << "[VIDEO_FILE] ~ " << std::format(src, __VA_ARGS__) << std::endl

namespace ReplayClipper {

    VideoFile::VideoFile() :
            m_Context(std::make_unique<ffmpeg_context_t>()),
            m_VideoStream(std::make_unique<ffmpeg_stream_t>()),
            m_Stream(std::make_unique<ffmpeg_packet_t>()) {

        m_Context->FormatContext = avformat_alloc_context();
        m_Stream->Frame = av_frame_alloc();
        m_Stream->Packet = av_packet_alloc();
        assert(m_Stream->Frame && m_Stream->Packet && "Failed to allocate frame or packet");

    }

    VideoFile::~VideoFile() {
        avformat_close_input(&m_Context->FormatContext);
        av_packet_free(&m_Stream->Packet);
        av_frame_free(&m_Stream->Frame);
        avcodec_free_context(&m_VideoStream->CodecContext);
    }

    bool VideoFile::OpenFile(const fs::path& path) {

        // Open File
        auto str = path.generic_string();
        if (avformat_open_input(&m_Context->FormatContext, str.c_str(), nullptr, nullptr) != 0) {
            VF_LOG("Failed to open input '{}'", str);
            return false;
        }

        // Process Stream
        if (avformat_find_stream_info(m_Context->FormatContext, nullptr) < 0) {
            VF_LOG("Failed to process stream info");
            return false;
        }

        for (int i = 0; i < m_Context->FormatContext->nb_streams; ++i) {
            AVCodecParameters* params = m_Context->FormatContext->streams[i]->codecpar;
            const AVCodec* codec = avcodec_find_decoder(params->codec_id);

            if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
                m_VideoStream->StreamIndex = i;
                m_VideoStream->CodecContext = avcodec_alloc_context3(codec);

                if (avcodec_parameters_to_context(m_VideoStream->CodecContext, params) < 0) {
                    VF_LOG("Failed to copy codec parameters to codec context");
                    return false;
                }

                if (avcodec_open2(m_VideoStream->CodecContext, codec, nullptr) < 0) {
                    VF_LOG("Failed to open codec {}", codec->long_name);
                    return false;
                }

                VF_LOG("VideoStream ~ {}, {}", i, codec->long_name);
            }
        }

        return true;
    }

    void VideoFile::Seek(double seconds) {
        int res = av_seek_frame(m_Context->FormatContext, -1, AV_TIME_BASE * seconds, AVSEEK_FLAG_BACKWARD);
        if (res < 0) {
            VF_LOG("Failed to seek to '{}'", seconds);
        }
    }

    AVFrame* VideoFile::NextFrame() {

        while (av_read_frame(m_Context->FormatContext, m_Stream->Packet) >= 0) {

            if (m_Stream->Packet->stream_index == m_VideoStream->StreamIndex) {
                int res = avcodec_send_packet(m_VideoStream->CodecContext, m_Stream->Packet);

                if (res < 0) {
                    VF_LOG("Error in sending packet");
                    return nullptr;
                }

                while (res >= 0) {
                    res = avcodec_receive_frame(m_VideoStream->CodecContext, m_Stream->Frame);

                    // EAGAIN means we need more packets to get a full frame
                    // EOF just means that there is no more frames and or packets
                    if (res == AVERROR(EAGAIN) || res == AVERROR_EOF) {
                        break;
                    } else if (res < 0) {
                        VF_LOG("Failure in receiving frame");
                        return nullptr;
                    }

                    if (res >= 0) {
                        VF_LOG("[Frame_{}] ~ {}, {}x{}",
                               m_VideoStream->CodecContext->frame_num,
                               av_get_pix_fmt_name((AVPixelFormat) m_Stream->Frame->format),
                               m_Stream->Frame->width,
                               m_Stream->Frame->height
                        );
                        av_packet_unref(m_Stream->Packet);
                        return m_Stream->Frame;
                    }
                }
            }
            av_packet_unref(m_Stream->Packet);
        }

        return nullptr;
    }

} // ReplayClipper