//
// Date       : 01/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_CLIPPER_H
#define REPLAYCLIPPER_CLIPPER_H

#include "Application.h"
#include "AudioPlayer.h"
#include "VideoStream.h"

namespace ReplayClipper {

    class Clipper : public Application {

      private:
        AudioPlayer m_Player;
        VideoStream m_Stream;
        uint64_t m_Elapsed;
        Frame m_CurrentFrame;

      public:
        unsigned int m_FrontTexture;

      public:
        virtual ~Clipper() override = default;
        virtual int Start() override;

      protected:
        virtual void OnImGui(float ts) override;
        virtual void OnStart() override;
        virtual bool OnUpdate(float ts) override;
        virtual void OnShutdown() override;

      private:
        void SetupDefaultDockspace();

      protected:
        static void ProcessVideo(Clipper* app);
    };

} // ReplayClipper

#endif
