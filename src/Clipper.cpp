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

    // TODO: Replace UI Code with function calls for clarity.
    // TODO: Clipping Jobs
    // TODO: Move the Windows API usage out

    // @off
    static int      width            = 0;
    static int      height           = 0;
    static uint64_t video_timestamp  = 0;
    static double   video_frame_size = 0;

    static int      channel          = 0;
    static int      sample_rate      = 0;
    static uint64_t audio_timestamp  = 0;
    static double   audio_frame_size = 0;

    static double video_frame_upload_time  = 0.0;
    static double video_frame_seek_time    = 0.0;
    static double audio_frame_enqueue_time = 0.0;
    static double audio_frame_seek_time    = 0.0;

    // @on

    int Clipper::Start() {
        return StartInternal(800, 600, "ReplayClipper");
    }

    void Clipper::OnStart() {

        glGenTextures(1, &m_FrontTexture);
        glBindTexture(GL_TEXTURE_2D, m_FrontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        REPLAY_TRACE("Working Dir: '{}'", fs::absolute(m_DirNavigator.GetParent()).generic_string());

        m_Stream.OpenStream("res/1.mp4");
        m_CurrentFrame = m_Stream.NextFrame();

        m_Player.OpenStream(m_Stream.GetChannels(), m_Stream.GetSampleRate());
        m_Player.Play();
    }

    void Clipper::OnImGui(float ts) {
        this->Application::OnImGui(ts);

        SetupDefaultDockspace();

        ShowFileTreeNavigator();
        ShowAppMetrics();

        ImGui::End();
        {

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
                                ((window_size.y - display.y) * 0.5F)
                                + ImGui::GetStyle().WindowPadding.y,
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

    //############################################################################//
    // | UI |
    //############################################################################//

    void Clipper::ShowAppMetrics() {
        if (ImGui::Begin("Metrics")) {

            // VIDEO METRICS
            if (m_CurrentFrame.IsVideo()) {
                const Frame::video_frame_t& video = m_CurrentFrame;
                width = video.Width;
                height = video.Height;
                video_timestamp = video.Timestamp;
                video_frame_size = ((double(video.Pixels.size()) / 1024.0) / 1024.0);

                // AUDIO METRICS
            } else if (m_CurrentFrame.IsAudio()) {
                const Frame::audio_frame_t& audio = m_CurrentFrame;
                channel = audio.Channels;
                sample_rate = audio.SampleRate;
                audio_timestamp = audio.Timestamp;
                audio_frame_size = ((double(audio.Samples.size()) / 1024.0) / 1024.0);
            }

            auto append_table = [](const char* header, const char* fmt, auto&& value) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None);
                ImGui::TableNextColumn();
                ImGui::Text("%s", header);
                ImGui::TableNextColumn();
                ImGui::Text(fmt, value);
            };

            auto append_header_row = [](const char* header) {
                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4{0.15F, 0.65F, 0.89F, 1.0F}, "%s", header);
                ImGui::TableSetColumnIndex(1);
            };

            if (ImGui::BeginTable("Timings", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {

                append_header_row("Video Metrics");
                append_table("Video Time", "%llu", m_Elapsed);

                append_header_row("Video Frame");
                append_table("Width", "%d", width);
                append_table("Height", "%d", height);
                append_table("Timestamp", "%llu", video_timestamp);
                append_table("Size", "%.3fmb", video_frame_size);

                append_header_row("Audio Frame");
                append_table("Channels", "%d", channel);
                append_table("SampleRate", "%d", sample_rate);
                append_table("Timestamp", "%llu", audio_timestamp);
                append_table("Size", "%.3fmb", audio_frame_size);

                append_header_row("Frame Timings");
                append_table("Is Stream Open", "%s", m_Stream.IsOpen() ? "Yes" : "No");
                append_table("Video Frame Upload Time", "%.3fms", video_frame_upload_time);
                append_table("Video Frame Seek Time", "%.3fms", video_frame_seek_time);
                append_table("Total Video Frame Time", "%.3fms", video_frame_seek_time + video_frame_upload_time);
                append_table("Audio Frame Enqueue Time", "%.3fms", audio_frame_enqueue_time);
                append_table("Audio Frame Seek Time", "%.3fms", audio_frame_seek_time);
                append_table("Total Audio Frame Time", "%.3fms", audio_frame_seek_time + audio_frame_enqueue_time);

                ImGui::EndTable();
            }
        }
    }

    void Clipper::ShowFileTreeNavigator() {
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
                            m_DirNavigator.SetRoot(fs::path{pszFilePath});

                            // Cleanup
                            CoTaskMemFree(pszFilePath);
                            psiResult->Release();
                        }
                    }
                    pfd->Release();
                }
            }

            {
                struct TreeNode {
                    size_t Index;
                    bool IsSelected;
                };

                static fs::path new_root = fs::path{};
                static bool has_new_root = false;

                ImGui::SameLine();
                if (ImGui::Button("Up")) {
                    new_root = m_DirNavigator.GetParent();
                    has_new_root = new_root != fs::path{};
                }

                if (has_new_root) {
                    m_DirNavigator.SetRoot(new_root);
                }

                new_root = fs::path{};
                has_new_root = false;

                const fs::path& root = m_DirNavigator.GetParent();
                std::string root_name = root.filename().generic_string();

                ImGui::SetNextItemOpen(true);
                if (ImGui::TreeNode(root_name.c_str())) {

                    for (size_t i = 0; i < m_DirNavigator.ChildrenCount(); ++i) {
                        const fs::path& child = m_DirNavigator[i];
                        std::string child_name = child.filename().generic_string();

                        int child_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (fs::is_regular_file(child)) {
                            child_flags |= ImGuiTreeNodeFlags_Leaf;
                        }

                        ImGui::SetNextItemOpen(false);
                        if (ImGui::TreeNodeEx(child_name.c_str(), child_flags)) {

                            if (ImGui::IsItemToggledOpen() && fs::is_directory(child)) {
                                new_root = child;
                                has_new_root = true;

                            } else if (ImGui::IsItemClicked() && fs::is_regular_file(child)) {
                                // TODO: Cleanup
                                if (m_Stream.OpenStream(child)) {
                                    m_Elapsed = 0;
                                    m_CurrentFrame = Frame{};
                                    do {
                                        m_CurrentFrame = m_Stream.NextFrame();
                                    } while (!m_CurrentFrame.IsValid());

                                    m_Player.CloseStream();
                                    m_Player.OpenStream(m_Stream.GetChannels(), m_Stream.GetSampleRate());
                                    m_Player.Resume();
                                }

                            }

                            ImGui::TreePop();
                        }

                    }

                    ImGui::TreePop();
                }
            }
        }
        ImGui::End();
    }
} // ReplayClipper
