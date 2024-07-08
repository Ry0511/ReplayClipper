//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "Clipper.h"
#include "FileTree.h"
#include "VideoFile.h"

#include <string>
#include <iostream>

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

        glGenTextures(1, &m_BackTexture);
        glBindTexture(GL_TEXTURE_2D, m_BackTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        m_ShutdownSignal = false;
        m_VideoTime = 0.0F;
        m_CopyToFront = false;
        m_PixelData = {};
        bool ok = m_Video.OpenFile("res/1.mp4");
        assert(ok && "Failed to open file");

        m_VideoProcessingThread = std::thread{this->ProcessVideo, this};

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
                        std::unique_lock guard{m_VideoMutex};
                        bool success = m_Video.OpenFile(path);
                        assert(success);
                    }
            );
        }
        ImGui::End();

        {
            std::unique_lock guard{m_VideoMutex};
            m_VideoTime += ts;
            if (ImGui::Begin("Video Player")) {

                if (m_CopyToFront) {
                    glBindTexture(GL_TEXTURE_2D, m_FrontTexture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_Width, m_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, m_PixelData.data());
                    glBindTexture(GL_TEXTURE_2D, 0);
                    m_CopyToFront = false;
                }

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
        m_ShutdownSignal = true;
        m_VideoProcessingThread.join();
        unsigned int textures[]{m_FrontTexture, m_BackTexture};
        glDeleteTextures(2, textures);
    }

    void Clipper::ProcessVideo(Clipper* clipper) {

        while (!clipper->m_ShutdownSignal) {
            constexpr float FRAMERATE = 1.0F / 60.0F;

            while (clipper->m_CopyToFront) {
                std::this_thread::yield();
            }

            while (clipper->m_VideoTime >= FRAMERATE) {
                clipper->m_VideoTime -= FRAMERATE;
                std::unique_lock guard{clipper->m_VideoMutex};

                VideoFile::Frame frame = clipper->m_Video.NextFrame();

                while (!frame.IsVideoFrame()) {
                    if (frame.IsEOF() || !frame.IsValid()) {
                        clipper->m_Video.Seek(0.0);
                    }
                    frame = clipper->m_Video.NextFrame();
                }

                frame.CopyInto(clipper->m_PixelData);
                clipper->m_Width = frame.Width();
                clipper->m_Height = frame.Height();
                clipper->m_CopyToFront = true;
            }
        }
    }

} // ReplayClipper