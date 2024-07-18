//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "Clipper.h"
#include "FileTree.h"

#include <string>

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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        assert(m_Stream.OpenStream("res/1.mp4"));
        m_CurrentFrame = m_Stream.NextFrame();

        assert(m_Player.OpenStream(2, 48000U));
        m_Player.Play();
        assert(m_Player.IsStreamOpen());
    }

    void Clipper::OnImGui(float ts) {
        this->Application::OnImGui(ts);

        if (ImGui::Begin("File Tree")) {
            struct {
                void ImFileTree(const FileTree::Node& node, std::function<void(const fs::path&)> fn) {

                    constexpr auto LEAF_NODE_FLAGS = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    std::string node_path = node.Path.filename().generic_string();

                    if (ImGui::TreeNode(node_path.c_str())) {
                        for (const FileTree::Node& child : node.Children) {
                            if (fs::is_regular_file(child.Path)) {
                                std::string child_path = child.Path.filename().generic_string();
                                if (ImGui::TreeNodeEx(child_path.c_str(), LEAF_NODE_FLAGS)) {
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
                    }
            );
        }
        ImGui::End();

        if (ImGui::Begin("Metrics")) {
            ImGui::Text("Video Time %llu", m_Elapsed);

            static int width = 0, height = 0;
            static uint64_t video_timestamp = 0;

            static int channel = 0, sample_rate = 0;
            static uint64_t audio_timestamp = 0;

            if (m_CurrentFrame.IsVideo()) {
                const Frame::video_frame_t& video = m_CurrentFrame;
                width = video.Width;
                height = video.Height;
                video_timestamp = video.Timestamp;

            } else if (m_CurrentFrame.IsAudio()) {
                const Frame::audio_frame_t& audio = m_CurrentFrame;
                channel = audio.Channels;
                sample_rate = audio.SampleRate;
                audio_timestamp = audio.Timestamp;
            }

            ImGui::SeparatorText("Video");
            ImGui::Indent(8.0F);
            ImGui::Text("Width     %d", width);
            ImGui::Text("Height    %d", height);
            ImGui::Text("Timestamp %llu", video_timestamp);
            ImGui::Unindent(8.0F);

            ImGui::SeparatorText("Audio");
            ImGui::Indent(8.0F);
            ImGui::Text("Channels    %d", channel);
            ImGui::Text("Sample Rate %d", sample_rate);
            ImGui::Text("Timestamp   %llu", audio_timestamp);
            ImGui::Unindent(8.0F);
        }
        ImGui::End();

        {
            m_Elapsed += static_cast<uint64_t>(ts * 1e6);
            if (ImGui::Begin("Video Player")) {

                // VIDEO
                if (m_CurrentFrame.IsVideo()) {

                    if (m_Elapsed >= m_CurrentFrame.Timestamp()) {
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

                        m_CurrentFrame = m_Stream.NextFrame();
                    }

                    // AUDIO
                } else if (m_CurrentFrame.IsAudio()) {
                    if (m_Elapsed >= m_CurrentFrame.Timestamp()) {
                        Frame::audio_frame_t& audio = m_CurrentFrame;
                        m_Player.EnqueueOnce(std::move(audio.Samples));
                        m_CurrentFrame = m_Stream.NextFrame();
                    }

                } else if (!m_CurrentFrame.IsValid() && m_Stream.IsOpen()) {
                    m_CurrentFrame = m_Stream.NextFrame();
                }

                // Display Current Video Frame
                ImGui::Image((void*) (intptr_t) m_FrontTexture, ImGui::GetContentRegionAvail());
            }
            ImGui::End();
        }

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

    void Clipper::ProcessVideo(Clipper* clipper) {

    }

} // ReplayClipper