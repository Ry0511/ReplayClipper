//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_CLIPPER_H
#define REPLAYCLIPPER_CLIPPER_H

#include "Application.h"
#include "VideoFile.h"

namespace ReplayClipper {

    class Clipper : public Application {

      private:
        VideoFile m_Video;
        float m_VideoTime;

      public:
        std::thread m_VideoProcessingThread;
        bool m_ShutdownSignal;
        std::vector<Pixel> m_PixelData;
        bool m_CopyToFront;
        int m_Width, m_Height;
        unsigned int m_FrontTexture, m_BackTexture;

      public:
        virtual ~Clipper() override = default;
        virtual int Start() override;

      protected:
        virtual void OnImGui(float ts) override;
        virtual void OnStart() override;
        virtual bool OnUpdate(float ts) override;
        virtual void OnShutdown() override;

      protected:
        static void ProcessVideo(Clipper* app);
    };

} // ReplayClipper

#endif
