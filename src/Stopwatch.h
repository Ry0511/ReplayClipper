//
// Date       : 20/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//

#ifndef REPLAYCLIPPER_STOPWATCH_H
#define REPLAYCLIPPER_STOPWATCH_H

#include <chrono>

namespace ReplayClipper {

    class Stopwatch {

      private:
        using Clock = std::chrono::steady_clock;
        using Instant = Clock::time_point;
        using Duration = Clock::duration;

      private:
        Instant m_Start;
        Instant m_End;

      public:
        explicit Stopwatch() : m_Start(Clock::now()), m_End(m_Start) {};
        ~Stopwatch() = default;

      public:
        inline void Start() noexcept {
            m_Start = Clock::now();
            m_End = m_Start;
        }
        inline void End() noexcept {
            m_End = Clock::now();
        }
        inline void Stop() noexcept {
            End();
        }

      public:
        Duration Elapsed() const noexcept {
            return m_End - m_Start;
        }

      private:
        template<class T, class TimeScale>
        T GetTimeScaled() const noexcept {
            return std::chrono::duration<T, typename TimeScale::period>(m_End - m_Start).count();
        }

      public:
        template<class T = float>
        requires(std::is_arithmetic_v<T>)
        T Seconds() const noexcept {
            return GetTimeScaled<T, std::chrono::seconds>();
        }

        template<class T = int>
        requires(std::is_arithmetic_v<T>)
        T Millis() const noexcept {
            return GetTimeScaled<T, std::chrono::milliseconds>();
        }

        template<class T = int>
        requires(std::is_arithmetic_v<T>)
        T Micro() const noexcept {
            return GetTimeScaled<T, std::chrono::microseconds>();
        }

        template<class T = long long>
        requires(std::is_arithmetic_v<T>)
        T Nano() const noexcept {
            return GetTimeScaled<T, std::chrono::nanoseconds>();
        }

    };

} // ReplayClipper

#endif
