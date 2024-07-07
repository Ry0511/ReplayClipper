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
        fs::path working_dir = fs::absolute(fs::path{"../"}).parent_path();
        fs::path bin = working_dir;

        bool bin_not_found = true;
        while (bin.has_parent_path() && bin_not_found) {
            std::cout << "[DIR] ~ " << bin.filename() << std::endl;
            bin = bin.parent_path();
            bin_not_found = (bin.filename() == "bin");
        }

        assert(!working_dir.filename().empty());

        if (bin_not_found) {
            gTree.InitRoot(working_dir);
        } else {
            gTree.InitRoot(bin / "bin");
        }

        glGenTextures(1, &m_Texture);
        glBindTexture(GL_TEXTURE_2D, m_Texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        bool ok = m_Video.OpenFile("res/1.mp4");
        assert(ok && "Failed to open file");
    }

    void Clipper::OnImGui(float ts) {


        if (ImGui::Begin("File Tree")) {
            struct {
                void ImFileTree(const FileTree::Node& node) {

                    constexpr auto LEAF_NODE_FLAGS = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    std::string node_path = node.Path.filename().generic_string();

                    if (ImGui::TreeNode(node_path.c_str())) {
                        for (const FileTree::Node& child : node.Children) {
                            if (fs::is_regular_file(child.Path)) {
                                std::string child_path = child.Path.filename().generic_string();
                                if (ImGui::TreeNodeEx(child_path.c_str(), LEAF_NODE_FLAGS)) {
                                    if (ImGui::IsItemClicked()) {
                                        std::cout << "Clicked ~ '" << child.Path.generic_string() << "'" << std::endl;
                                    }
                                }
                            } else {
                                ImFileTree(child);
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            } walker;

            walker.ImFileTree(gTree.Root());
        }
        ImGui::End();

        m_VideoTime += ts;

        constexpr float FRAMERATE = 1.0F / 60.0F;
        while (m_VideoTime >= FRAMERATE) {
            m_VideoTime -= FRAMERATE;
            AVFrame* frame = m_Video.NextFrame();

            if (frame == nullptr) {
                std::cout << "End of video " << m_VideoTime << std::endl;
                m_VideoTime = 0.0F;
                m_Video.Seek(0.0);
                frame = m_Video.NextFrame();
                assert(frame && "Video Frame should not be null");
            }

            if (m_VideoTime >= FRAMERATE) {
                continue;
            }

            glBindTexture(GL_TEXTURE_2D, m_Texture);
            int width = frame->width;
            int height = frame->height;
            uint8_t* buf = frame->data[0];

            struct Pixel {
                uint8_t Red, Green, Blue;
            };

            static std::vector<Pixel> pixels{};
            pixels.resize(width * height);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t index = frame->linesize[0] * y + x;
                    uint8_t g = buf[index];
                    pixels[index] = Pixel{g, g, g};
                }
            }

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        if (ImGui::Begin("Video Player")) {
            ImGui::Image((void*) (intptr_t) m_Texture, ImGui::GetContentRegionAvail());
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

    }

} // ReplayClipper