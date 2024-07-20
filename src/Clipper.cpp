//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "Clipper.h"
#include "FileTree.h"
#include "Logging.h"

#include <string>
#include <imgui_internal.h>

#include <windows.h>
#include <ShlObj.h>

namespace ReplayClipper {

    // TODO: Find a good way to deal with time units. Likely using nanoseconds internally for
    //  everything. Input functions will just assume you provided nanoseconds and rescale internally.

    static FileTree gTree;

    int Clipper::Start() {
        return StartInternal(800, 600, "ReplayClipper");
    }

    void Clipper::OnStart() {
        fs::path working_dir = fs::absolute(fs::path{"./"}).parent_path();
        gTree.InitRoot(working_dir / "res");

        glGenTextures(1, &m_FrontTexture);
        glBindTexture(GL_TEXTURE_2D, m_FrontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        m_Stream.OpenStream("res/1.mp4");
        m_CurrentFrame = m_Stream.NextFrame();

        m_Player.OpenStream(m_Stream.GetChannels(), m_Stream.GetSampleRate());
        m_Player.Play();
    }

    void Clipper::OnImGui(float ts) {
        this->Application::OnImGui(ts);

        SetupDefaultDockspace();

        if (ImGui::Begin("File Tree")) {
            if (ImGui::Button("Open Directory")) {
                // TODO: Extract to function
                IFileDialog* pfd = nullptr;

                // CoCreateInstance for IFileDialog
                HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
                if (SUCCEEDED(hr)) {
                    // Set dialog options
                    DWORD dwOptions;
                    pfd->GetOptions(&dwOptions);
                    pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);

                    // Show the dialog
                    hr = pfd->Show(NULL);
                    if (SUCCEEDED(hr)) {
                        // Get the result
                        IShellItem* psiResult = nullptr;
                        hr = pfd->GetResult(&psiResult);
                        if (SUCCEEDED(hr)) {
                            // Get the file path
                            PWSTR pszFilePath = nullptr;
                            psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                            // Output the selected directory path
                            gTree.InitRoot(fs::path{pszFilePath});

                            // Cleanup
                            CoTaskMemFree(pszFilePath);
                            psiResult->Release();
                        }
                    }
                    pfd->Release();
                }
            }

            struct {
                void ImFileTree(const FileTree::Node& node, std::function<void(const fs::path&)> fn) {

                    constexpr auto LEAF_NODE_FLAGS = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    std::string node_path = node.Path.filename().generic_string();

                    if (ImGui::TreeNode(node_path.c_str())) {
                        for (const FileTree::Node& child : node.Children) {
                            if (fs::is_regular_file(child.Path)) {
                                std::string child_path = child.Path.filename().generic_string();
                                if (ImGui::TreeNodeEx(child_path.c_str(), LEAF_NODE_FLAGS | ImGuiTreeNodeFlags_SpanFullWidth)) {
                                    if (ImGui::IsItemClicked()) {
                                        fn(child.Path);
                                    }
                                }
                            } else {
                                ImFileTree(child, fn);
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            } walker;

            walker.ImFileTree(
                    gTree.Root(),
                    [this](const fs::path& path) {
                        if (!fs::is_regular_file(path) || path.extension() != ".mp4") {
                            return;
                        }
                        bool success = m_Stream.OpenStream(path);
                        assert(success);
                        m_Elapsed = 0;
                        m_CurrentFrame = m_Stream.NextFrame();

                        m_Player.CloseStream();
                        m_Player.OpenStream(m_Stream.GetChannels(), m_Stream.GetSampleRate());
                        m_Player.Resume();
                    }
            );
        }
        ImGui::End();

        if (ImGui::Begin("Metrics")) {
            ImGui::TextWrapped("Video Time %llu", m_Elapsed);

            static int width = 0, height = 0;
            static uint64_t video_timestamp = 0;
            static double video_frame_size = 0;

            static int channel = 0, sample_rate = 0;
            static uint64_t audio_timestamp = 0;
            static double audio_frame_size = 0;

            if (m_CurrentFrame.IsVideo()) {
                const Frame::video_frame_t& video = m_CurrentFrame;
                width = video.Width;
                height = video.Height;
                video_timestamp = video.Timestamp;
                video_frame_size = ((double(video.Pixels.size()) / 1024.0) / 1024.0);

            } else if (m_CurrentFrame.IsAudio()) {
                const Frame::audio_frame_t& audio = m_CurrentFrame;
                channel = audio.Channels;
                sample_rate = audio.SampleRate;
                audio_timestamp = audio.Timestamp;
                audio_frame_size = ((double(audio.Samples.size()) / 1024.0) / 1024.0);
            }

            ImGui::SeparatorText("Video");
            ImGui::Text("Width     %d", width);
            ImGui::Text("Height    %d", height);
            ImGui::Text("Timestamp %llu", video_timestamp);
            ImGui::Text("Size      %.3fmb", video_frame_size);

            ImGui::SeparatorText("Audio");
            ImGui::Text("Channels    %d", channel);
            ImGui::Text("Sample Rate %d", sample_rate);
            ImGui::Text("Timestamp   %llu", audio_timestamp);
            ImGui::Text("Size      %.3fmb", audio_frame_size);
        }
        ImGui::End();

        { // @off
            static double video_frame_upload_time  = 0.0;
            static double video_frame_seek_time    = 0.0;
            static double audio_frame_enqueue_time = 0.0;
            static double audio_frame_seek_time    = 0.0;
          // @on

            m_Elapsed += static_cast<uint64_t>(ts * 1e6);
            if (ImGui::Begin("Video Player")) {
                Stopwatch watch{};

                bool frame_consumed = false;

                // VIDEO
                if (m_CurrentFrame.IsVideo() && m_Elapsed >= m_CurrentFrame.Timestamp()) {
                    watch.Start();
                    const Frame::video_frame_t& video = m_CurrentFrame;
                    glBindTexture(GL_TEXTURE_2D, m_FrontTexture);
                    glTexImage2D(
                            GL_TEXTURE_2D, 0, GL_RGBA,
                            video.Width, video.Height,
                            0,
                            GL_RGB,
                            GL_UNSIGNED_BYTE,
                            video.Pixels.data()
                    );
                    glBindTexture(GL_TEXTURE_2D, 0);
                    m_Width = video.Width;
                    m_Height = video.Height;
                    watch.End();
                    video_frame_upload_time = watch.Millis<double>();

                    frame_consumed = true;

                    // AUDIO
                } else if (m_CurrentFrame.IsAudio()) {
                    watch.Start();
                    Frame::audio_frame_t& audio = m_CurrentFrame;
                    m_Player.EnqueueOnce(std::move(audio.Samples));
                    watch.End();
                    audio_frame_enqueue_time = watch.Millis<double>();

                    frame_consumed = true;

                }

                if (frame_consumed) {
                    watch.Start();
                    m_CurrentFrame = m_Stream.NextFrame();
                    watch.End();

                    if (m_CurrentFrame.IsVideo()) {
                        video_frame_seek_time = watch.Millis<double>();
                    } else if (m_CurrentFrame.IsAudio()) {
                        audio_frame_seek_time = watch.Millis<double>();
                    }
                }

                if (ImGui::Begin("Metrics")) {
                    ImGui::TextWrapped("Stream Open ~ %s", m_Stream.IsOpen() ? "Yes" : "No");
                    ImGui::SeparatorText("Timings");
                    ImGui::Text("Video Frame Upload Time  %.3fms", video_frame_upload_time);
                    ImGui::Text("Video Frame Seek Time    %.3fms", video_frame_seek_time);
                    ImGui::Text("Total Video Frame Time   %.3fms", video_frame_seek_time + video_frame_upload_time);
                    ImGui::Text("Audio Frame Enqueue Time %.3fms", audio_frame_enqueue_time);
                    ImGui::Text("Audio Frame Seek Time    %.3fms", audio_frame_seek_time);
                    ImGui::Text("Total Audio Frame Time   %.3fms", audio_frame_seek_time + audio_frame_enqueue_time);
                }
                ImGui::End();

                // Display Current Video Frame
                double aspect = double(m_Width) / double(m_Height);
                ImVec2 space = ImGui::GetContentRegionAvail();
                ImVec2 display{0, 0};

                if (float(space.x) / float(space.y) > aspect) {
                    display.y = space.y;
                    display.x = display.y * aspect;
                } else {
                    display.x = space.x;
                    display.y = display.x / aspect;
                }

                ImVec2 original_pos = ImGui::GetCursorScreenPos();
                ImVec2 window_pos = ImGui::GetWindowPos();
                ImVec2 window_size = ImGui::GetWindowSize();

                ImGui::SetCursorPos(
                        ImVec2{
                                (window_size.x - display.x) * 0.5F,
                                ((window_size.y - display.y) * 0.5F) + ImGui::GetStyle().WindowPadding.y,
                        }
                );

                ImGui::Image((void*) (intptr_t) m_FrontTexture, display);

                ImGui::SetCursorScreenPos(original_pos);
            }
            ImGui::End();
        }

        if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGuiDockNode* node = ImGui::GetWindowDockNode();

            if (node) {
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
                node->LocalFlags |= ImGuiDockNodeFlags_NoResize;
            }

            ImVec2 avail = ImGui::GetContentRegionAvail();
            avail.x -= ImGui::GetStyle().CellPadding.x * 2.0F;

            ImVec2 cursor_start = ImGui::GetCursorPos();
            int64_t duration = m_Stream.GetDuration();
            float progress = 0.0F;
            progress = (m_Elapsed >= duration)
                       ? 1.0F
                       : double(m_Elapsed) / double(duration);
            ImGui::ProgressBar(progress, ImVec2{avail.x * 0.75F, 0}, "");
            if (ImGui::IsItemClicked()) {
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                ImVec2 pos = ImGui::GetMousePos();

                double normalized_click_pos = (pos.x - min.x) / (max.x - min.x);
                normalized_click_pos = std::clamp(normalized_click_pos, 0.0, 1.0);

                double new_playback_position = duration * normalized_click_pos;
                REPLAY_TRACE("Seek Pos {}, {}", normalized_click_pos, new_playback_position / 1e6F);
                m_Stream.Seek(new_playback_position / 1e6F);

                m_CurrentFrame = m_Stream.NextFrame();
                while (!m_CurrentFrame.IsValid()) {
                    m_CurrentFrame = m_Stream.NextFrame();
                }
                m_Elapsed = m_CurrentFrame.Timestamp();
                m_Player.ClearQueue();
            }

            ImGui::SameLine();
            static float volume = m_Player.GetVolumeScale();
            ImGui::SetNextItemWidth(avail.x * 0.25F);
            if (ImGui::SliderFloat("##", &volume, 0.0F, 1.0F, "Volume %.2f", ImGuiSliderFlags_AlwaysClamp)) {
                m_Player.SetVolumeScale(volume);
            }

            ImVec2 cursor_end = ImGui::GetCursorPos();
            ImVec2 required_size = ImVec2(cursor_end.x - cursor_start.x + 16.0F, cursor_end.y - cursor_start.y + 16.0F);
            if (node && node->ParentNode) {
                node->Size = required_size;
                node->SizeRef = required_size;
            }
        }
        ImGui::End();

        if (ImGui::Begin("Jobs")) {

        }
        ImGui::End();

    }

    bool Clipper::OnUpdate(float ts) {
        return true;
    }

    void Clipper::OnShutdown() {
        glDeleteTextures(1, &m_FrontTexture);
    }

    void Clipper::SetupDefaultDockspace() {

    }

    void Clipper::ProcessVideo(Clipper* clipper) {

    }

} // ReplayClipper