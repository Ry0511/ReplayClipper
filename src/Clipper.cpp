//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "Clipper.h"
#include "FileTree.h"

#include <string>
#include <imgui_internal.h>

#include <windows.h>
#include <ShlObj.h>

namespace ReplayClipper {

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
            ImGui::Indent(8.0F);
            ImGui::TextWrapped("Width     %d", width);
            ImGui::TextWrapped("Height    %d", height);
            ImGui::TextWrapped("Timestamp %llu", video_timestamp);
            ImGui::TextWrapped("Size      %.3fmb", video_frame_size);
            ImGui::Unindent(8.0F);

            ImGui::SeparatorText("Audio");
            ImGui::Indent(8.0F);
            ImGui::TextWrapped("Channels    %d", channel);
            ImGui::TextWrapped("Sample Rate %d", sample_rate);
            ImGui::TextWrapped("Timestamp   %llu", audio_timestamp);
            ImGui::TextWrapped("Size      %.3fmb", audio_frame_size);
            ImGui::Unindent(8.0F);
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
                    ImGui::TextWrapped("Video Frame Upload Time  %.3fms", video_frame_upload_time);
                    ImGui::TextWrapped("Video Frame Seek Time    %.3fms", video_frame_seek_time);
                    ImGui::TextWrapped("Total Video Frame Time   %.3fms", video_frame_seek_time + video_frame_upload_time);
                    ImGui::TextWrapped("Audio Frame Enqueue Time %.3fms", audio_frame_enqueue_time);
                    ImGui::TextWrapped("Audio Frame Seek Time    %.3fms", audio_frame_seek_time);
                    ImGui::TextWrapped("Total Audio Frame Time   %.3fms", audio_frame_seek_time + audio_frame_enqueue_time);
                }
                ImGui::End();

                // Display Current Video Frame
                ImGui::Image((void*) (intptr_t) m_FrontTexture, ImGui::GetContentRegionAvail());
            }
            ImGui::End();
        }

        if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration)) {
            ImGuiDockNode* node = ImGui::GetWindowDockNode();

            if (node) {
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            }

            static float volume = 0.33F;
            if (ImGui::SliderFloat("Volume", &volume, 0.0F, 1.0F, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
                m_Player.SetVolumeScale(volume);
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