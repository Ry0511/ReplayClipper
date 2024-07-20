//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_APPLICATION_H
#define REPLAYCLIPPER_APPLICATION_H

#include "Stopwatch.h"

#include "glad/glad.h"

#define IMGUI_HAS_DOCK
#include "imgui.h"

#include "GLFW/glfw3.h"

namespace ReplayClipper {

    struct Metrics {
        float Framerate;
        size_t FrameCount;
    };

    class Application {

      private:
        GLFWwindow* m_Window;
        Metrics m_Metrics;

      public:
        static Application* s_Active;

      public:
        Application() = default;
        virtual ~Application() = default;

      protected:
        int StartInternal(int width, int height, const char* title);

      public:
        virtual int Start() {
            return StartInternal(800, 600, "Application");
        }

      public:
        inline const Metrics& GetMetrics() const noexcept {
            return m_Metrics;
        }

      public:
        void SetWindowSize(int width, int height);
        void SetWindowPos(int x, int y);

      protected:
        virtual void OnImGui(float ts);
        virtual void OnStart() = 0;
        virtual bool OnUpdate(float ts) = 0;
        virtual void OnShutdown() = 0;
    };

} // ReplayClipper

#endif
