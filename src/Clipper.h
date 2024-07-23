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
#include "DirectoryNavigator.h"

namespace ReplayClipper {

    class Clipper : public Application {

      private:
        AudioPlayer m_Player;
        DirectoryNavigator m_DirNavigator;

      private:
        VideoStream m_Stream;
        uint64_t m_Elapsed = 0;
        Frame m_CurrentFrame;
        int m_Width = 0;
        int m_Height = 0;

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

      private:
        void ShowAppMetrics();
        void ShowFileTreeNavigator();

      protected:
        static void ProcessVideo(Clipper* app);
    };
} // ReplayClipper

#endif
