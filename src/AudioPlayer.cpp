//
// Date       : 09/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "AudioPlayer.h"
#include "Logging.h"

namespace ReplayClipper {

    AudioPlayer::AudioPlayer()
            : m_Mutex(),
              m_Handle(RtAudio::UNSPECIFIED),
              m_AudioScalar(0.33F), // To avoid destroying your ears
              m_SamplesQueue() {
    }

    AudioPlayer::~AudioPlayer() {
        if (!IsStreamOpen()) {
            return;
        }
        m_Handle.closeStream();
    }

    bool AudioPlayer::OpenStream(int channels, int sample_rate) {
        if (IsStreamOpen()) {
            REPLAY_WARN("A stream is already open; Close it first.");
            return false;
        }

        // Params
        RtAudio::StreamParameters params{};
        params.deviceId = m_Handle.getDefaultOutputDevice();
        params.nChannels = channels;
        params.firstChannel = 0;

        // Potentially no Device
        if (params.deviceId == 0) {
            return false;
        }

        unsigned int buffer = 512;
        RtAudioErrorType err = m_Handle.openStream(
                &params,
                nullptr,
                RTAUDIO_FLOAT32,
                sample_rate,
                &buffer,
                &AudioCallback,
                this
        );

        m_Channels = params.nChannels;

        if (err != RtAudioErrorType::RTAUDIO_NO_ERROR) {
            REPLAY_WARN("Failed to open RtAudio Stream; Error code {}", (int) err);
            return false;
        }

        if (buffer != 512) {
            REPLAY_TRACE("Audio Stream Buffer Frames {}", buffer);
        }

        return true;
    }

    bool AudioPlayer::CloseStream() {
        if (!IsStreamOpen()) {
            return false;
        }
        m_Handle.closeStream();
        m_Channels = 0;
        m_SamplesQueue.clear();
        return true;
    }

    void AudioPlayer::EnqueueOnce(std::vector<uint8_t>&& samples) {
        std::unique_lock guard{m_Mutex};
        m_SamplesQueue.emplace_back(std::move(samples));
    }

    int AudioPlayer::AudioCallback(
            void* out_raw,
            void*,
            unsigned int frames,
            double time_seconds,
            RtAudioStreamStatus status,
            void* self_raw
    ) {

        if (status != 0) {
            return -1;
        }

        AudioPlayer* self = static_cast<AudioPlayer*>(self_raw);
        float* out = static_cast<float*>(out_raw);

        const size_t bytes_to_write = frames * self->m_Channels * sizeof(float);

        {
            std::unique_lock guard{self->m_Mutex};

            if (self->m_SamplesQueue.empty()) {
                std::memset(out_raw, 0, bytes_to_write);
                return 0;
            }

            size_t written = 0;
            size_t offset = 0;
            do {
                ByteStream& stream = self->m_SamplesQueue.front();
                written = stream.Fetch(reinterpret_cast<uint8_t*>(out) + offset, bytes_to_write - offset);
                offset += written;
                if (stream.Remaining() == 0) {
                    self->m_SamplesQueue.pop_front();
                }
            } while ((written + offset) < bytes_to_write && !self->m_SamplesQueue.empty());
        }

        return 0;
    }

    void AudioPlayer::Play() noexcept {
        m_Handle.startStream();
    }

    void AudioPlayer::Pause() noexcept {
        m_Handle.stopStream();
    }

    void AudioPlayer::Resume() noexcept {
        m_Handle.startStream();
    }

} // ReplayClipper