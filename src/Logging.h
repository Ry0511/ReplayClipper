//
// Date       : 14/07/2024
// Project    : ReplayClipper
// Author     : -Ry
//
#ifndef REPLAYCLIPPER_LOGGING_H
#define REPLAYCLIPPER_LOGGING_H

#include <string>
#include <format>
#include <iostream>
#include <cassert>

#define REPLAY_LOG(stream, level, msg) stream << std::format("{:<6} - {}", level, msg) << std::endl

#define REPLAY_INFO(msg, ...) REPLAY_LOG(std::cout, "INFO", std::format(msg, __VA_ARGS__))
#define REPLAY_TRACE(msg, ...) REPLAY_LOG(std::cout, "TRACE", std::format(msg, __VA_ARGS__))
#define REPLAY_WARN(msg, ...) REPLAY_LOG(std::cout, "WARN", std::format(msg, __VA_ARGS__))
#define REPLAY_ERROR(msg, ...) REPLAY_LOG(std::cout, "ERROR", std::format(msg, __VA_ARGS__))

#define REPLAY_ASSERT(p) do {    \
        if (!(p)) {                \
            assert(false && #p); \
        }                        \
    } while (false)

#endif
