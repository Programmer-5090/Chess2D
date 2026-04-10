#ifndef LOGGER_H
#define LOGGER_H

#include <cstdio>
#include <string>
#include <vector>

#ifdef CHESS2D_NO_LOGGING

namespace spdlog {
    namespace level {
        enum level_enum { debug, info, warn, err };
    }

    template <typename... Args>
    inline void debug(Args&&...) {}
    template <typename... Args>
    inline void info(Args&&...) {}
    template <typename... Args>
    inline void warn(Args&&...) {}
    template <typename... Args>
    inline void error(Args&&...) {}
}

#else
#include <spdlog/spdlog.h>
#endif

namespace ChessLog {
#ifdef CHESS2D_NO_LOGGING
    inline void init(const std::string& = "logs", spdlog::level::level_enum = spdlog::level::debug, bool = false) {}
    inline void shutdown() {}
    inline bool isInitialized() { return false; }
#else
    void init(const std::string& logDir = "logs", spdlog::level::level_enum level = spdlog::level::debug, bool logToConsole = false);
    void shutdown();
    bool isInitialized();
#endif

    namespace detail {
        template <typename... Args>
        inline std::string printfFormat(const char* fmt, Args... args) {
            const int size = std::snprintf(nullptr, 0, fmt, args...);
            if (size <= 0) return std::string(fmt);

            std::vector<char> buffer(static_cast<std::size_t>(size) + 1);
            std::snprintf(buffer.data(), buffer.size(), fmt, args...);
            return std::string(buffer.data(), static_cast<std::size_t>(size));
        }
    }
}

#define LOG_DEBUG(msg) ::spdlog::debug("{}", msg)
#define LOG_INFO(msg)  ::spdlog::info("{}", msg)
#define LOG_WARN(msg)  ::spdlog::warn("{}", msg)
#define LOG_ERROR(msg) ::spdlog::error("{}", msg)

#define LOG_DEBUG_F(fmt, ...) ::spdlog::debug("{}", ::ChessLog::detail::printfFormat(fmt, __VA_ARGS__))
#define LOG_INFO_F(fmt, ...)  ::spdlog::info("{}", ::ChessLog::detail::printfFormat(fmt, __VA_ARGS__))
#define LOG_WARN_F(fmt, ...)  ::spdlog::warn("{}", ::ChessLog::detail::printfFormat(fmt, __VA_ARGS__))
#define LOG_ERROR_F(fmt, ...) ::spdlog::error("{}", ::ChessLog::detail::printfFormat(fmt, __VA_ARGS__))

#endif // LOGGER_H
