//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "Application.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <chrono>
#include <vector>
#include <cassert>

namespace ReplayClipper {

    using Clock = std::chrono::steady_clock;
    using Instant = std::chrono::steady_clock::time_point;
    using FloatDuration = std::chrono::duration<float>;

    //############################################################################//
    // | APPLICATION |
    //############################################################################//

    Application* Application::s_Active = nullptr;

    static int Initialise(GLFWwindow** window, int width, int height, const char* title) noexcept {
        // Init GLFW & Create Window
        int success = glfwInit();
        assert(success == GLFW_TRUE);
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        *window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        assert(window);

        // Load GL
        glfwSwapInterval(1);
        glfwMakeContextCurrent(*window);
        success = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
        assert(success);

        // Load ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(*window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        return 0;
    }

    int Application::StartInternal(int width, int height, const char* title) {
        Initialise(&m_Window, width, height, title);
        s_Active = this;

        this->OnStart();

        Stopwatch watch{};

        double frame_rate_accum = 0.0;
        int frame_rate_tick = 0;

        while (!glfwWindowShouldClose(m_Window)) {

            watch.Start();

            glfwPollEvents();

            // Pre GL Stuff
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            int fb_width, fb_height;
            glfwGetFramebufferSize(m_Window, &fb_width, &fb_height);
            glViewport(0, 0, fb_width, fb_height);

            { // Primitive Frame Locking
                constexpr size_t TARGET_FRAMERATE = NANOSECONDS_SCALE / 120LLU;
                Stopwatch temp{};
                size_t error = m_Metrics.Delta;
                while (error < TARGET_FRAMERATE) {
                    temp.Start();
                    std::this_thread::yield();
                    temp.End();
                    error -= temp.Nano<size_t>();
                }
            }

            // ImGui
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::DockSpaceOverViewport();
            ImGui::ShowDemoWindow();

            this->OnImGui(m_Metrics.DeltaSeconds);
            ImGui::Render();

            // Update
            bool should_continue = this->OnUpdate(m_Metrics.DeltaSeconds);

            // Rendering
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(m_Window);

            watch.End();

            m_Metrics.Delta = watch.Nano<size_t>();
            m_Metrics.DeltaSeconds = watch.Seconds<float>();
            m_Metrics.Elapsed += m_Metrics.Delta;
            m_Metrics.FrameCount++;

            frame_rate_accum += m_Metrics.DeltaSeconds;
            frame_rate_tick++;
            m_Metrics.Framerate = 1.0 / (frame_rate_accum / double(frame_rate_tick));
            if (frame_rate_tick > 1000) {
                frame_rate_accum = 0.0;
                frame_rate_tick = 0;
            }

            if (!should_continue) break;
        }
        this->OnShutdown();

        // Shutdown
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_Window);
        glfwTerminate();

        return 0;
    }

    void Application::OnImGui(float ts) {
        if (ImGui::Begin("Metrics")) {
            if (ImGui::BeginTable("Timings", 2, ImGuiTableFlags_Resizable)) {
                auto append_table = [](const char* header, const char* fmt, auto&& value) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_None);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", header);
                    ImGui::TableNextColumn();
                    ImGui::Text(fmt, value);
                };

                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4{0.15F, 0.65F, 0.89F, 1.0F}, "Application Timings");

                append_table("Framerate", "%.2f", m_Metrics.Framerate);
                append_table("Frame Count", "%llu", m_Metrics.FrameCount);
                append_table("Elapsed", "%llu", m_Metrics.Elapsed);
                append_table("Delta", "%llu", m_Metrics.Delta);
                append_table("Delta Seconds", "%.2f", m_Metrics.DeltaSeconds);

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void Application::SetWindowSize(int width, int height) {

    }

    void Application::SetWindowPos(int x, int y) {

    }

} // ReplayClipper