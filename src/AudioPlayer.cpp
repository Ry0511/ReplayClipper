//
// Date       : 09/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "AudioPlayer.h"
#include "Logging.h"

namespace ReplayClipper {

    AudioPlayer::AudioPlayer()
            : m_Handle(RtAudio::UNSPECIFIED),
              m_Mutex(),
              m_Index(0),
              m_AudioQueues(),
              m_Channels(-1),
              m_AudioScalar(0.15F) {
        auto api = m_Handle.getApiDisplayName(m_Handle.getCurrentApi());
        REPLAY_TRACE("AudioPlayer using API ~ {}", api);
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

        if (channels <= 0 || sample_rate <= 0) {
            REPLAY_TRACE("Invalid Channel Count or Sample Rate; {}, {}", channels, sample_rate);
            return false;
        }

        // Params
        RtAudio::StreamParameters params{};
        params.deviceId = m_Handle.getDefaultOutputDevice();
        params.nChannels = channels;
        params.firstChannel = 0;

        // Potentially no Device
        if (params.deviceId == 0) {
            REPLAY_WARN("Failed to open AudioStream device is unknown; {}", m_Handle.getErrorText());
            return false;
        }

        REPLAY_TRACE(
                "Opening Stream; {}, {}, {}, {}",
                params.deviceId,
                params.nChannels,
                params.firstChannel,
                sample_rate
        );
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
        std::unique_lock guard{m_Mutex};
        if (!IsStreamOpen()) {
            return false;
        }
        m_Handle.closeStream();
        m_Channels = 0;
        m_AudioQueues[0].clear();
        m_AudioQueues[1].clear();
        return true;
    }

    void AudioPlayer::EnqueueOnce(std::vector<uint8_t>&& samples) {
        int queue_index = (m_Index.load() + 1) % 2;
        m_AudioQueues[queue_index].emplace_back(std::move(samples));
    }

    void AudioPlayer::Play() noexcept {
        std::unique_lock guard{m_Mutex};
        m_Handle.startStream();
    }

    void AudioPlayer::Pause() noexcept {
        std::unique_lock guard{m_Mutex};
        m_Handle.stopStream();
    }

    void AudioPlayer::Resume() noexcept {
        std::unique_lock guard{m_Mutex};
        m_Handle.startStream();
    }

    int AudioPlayer::AudioCallback(
            void* out_raw,
            void*,
            unsigned int frames,
            double time_seconds,
            RtAudioStreamStatus status,
            void* self_raw
    ) {
        AudioPlayer* self = static_cast<AudioPlayer*>(self_raw);
        float* out = static_cast<float*>(out_raw);

        if (status != 0) {
            REPLAY_TRACE(
                    "Audio Stream Exiting; Non-Zero Status {}; {}",
                    status,
                    self->m_Handle.getErrorText()
            );
            return -1;
        }

        // Pretty much exclusive access 99% of the time
        std::unique_lock guard{self->m_Mutex};

        const size_t bytes_to_write = frames * self->m_Channels * sizeof(float);

        int current_queue_index = self->m_Index.load();
        std::deque<ByteStream>& queue = self->m_AudioQueues[current_queue_index];

        {
            if (queue.empty()) {
                std::memset(out_raw, 0, bytes_to_write);
            } else {
                size_t written = 0;
                size_t offset = 0;

                do {
                    ByteStream& stream = queue.front();
                    written = stream.Fetch(reinterpret_cast<uint8_t*>(out) + offset, bytes_to_write - offset);
                    offset += written;
                    if (stream.Remaining() == 0) {
                        queue.pop_front();
                    }
                } while ((written + offset) < bytes_to_write && !queue.empty());

                for (int i = 0; i < frames * self->m_Channels; ++i) {
                    out[i] *= self->m_AudioScalar;
                }
            }
        }

        if (queue.empty()) {
            self->m_Index.store((current_queue_index + 1) % 2);
        }

        return 0;
    }

} // ReplayClipper