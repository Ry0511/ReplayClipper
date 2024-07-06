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
        unsigned int m_Texture;
        VideoFile m_Video;
        float m_VideoTime;

      public:
        virtual ~Clipper() override = default;
        virtual int Start() override;

      protected:
        virtual void OnImGui(float ts) override;
        virtual void OnStart() override;
        virtual bool OnUpdate(float ts) override;
        virtual void OnShutdown() override;
    };

} // ReplayClipper

#endif
