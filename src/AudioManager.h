//
// Date       : 09/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_AUDIOMANAGER_H
#define REPLAYCLIPPER_AUDIOMANAGER_H

#include <cstdint>

namespace ReplayClipper {

    class AudioManager {

      public:
        static bool Initialise();
        static void Terminate();

      public:
        static void Pause();
        static void Resume();

      public:
        static void EnqueueOnce(uint8_t* data, size_t length);

    };

} // ReplayClipper

#endif
