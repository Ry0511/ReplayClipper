//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#include "Application.h"

#include "cassert"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <chrono>
#include <vector>

namespace ReplayClipper {

    using Clock = std::chrono::steady_clock;
    using Instant = std::chrono::steady_clock::time_point;
    using FloatDuration = std::chrono::duration<float>;

    //############################################################################//
    // | APPLICATION |
    //############################################################################//

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

        Instant before = Clock::now();
        Instant now = Clock::now();

        while (!glfwWindowShouldClose(m_Window)) {
            now = Clock::now();
            float ts = FloatDuration{now - before}.count();
            before = now;
            glfwPollEvents();

            // Pre GL Stuff
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            int fb_width, fb_height;
            glfwGetFramebufferSize(m_Window, &fb_width, &fb_height);
            glViewport(0, 0, fb_width, fb_height);

            // Metrics
            m_Metrics.FrameCount++;
            m_Metrics.Framerate = 1.0F / ts;

            // ImGui
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::DockSpaceOverViewport();
            ImGui::ShowDemoWindow();

            this->OnImGui(ts);
            ImGui::Render();

            // Update
            bool should_continue = this->OnUpdate(ts);

            // Rendering
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(m_Window);

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

    void Application::SetWindowSize(int width, int height) {

    }

    void Application::SetWindowPos(int x, int y) {

    }

} // ReplayClipper