#include <logger.h>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
    std::mutex g_logMutex;
    bool g_initialized = false;
}

void ChessLog::init(const std::string& logDir, spdlog::level::level_enum level, bool logToConsole) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_initialized) return;

    std::filesystem::create_directories(logDir);

    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif

    std::ostringstream fileName;
    fileName << logDir << "/chess2d_"
             << (tm.tm_year + 1900)
             << std::setw(2) << std::setfill('0') << (tm.tm_mon + 1)
             << std::setw(2) << std::setfill('0') << tm.tm_mday
             << "_"
             << std::setw(2) << std::setfill('0') << tm.tm_hour
             << std::setw(2) << std::setfill('0') << tm.tm_min
             << std::setw(2) << std::setfill('0') << tm.tm_sec
             << ".log";

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(fileName.str(), true);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(fileSink);
    if (logToConsole) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    auto logger = std::make_shared<spdlog::logger>("chess2d", sinks.begin(), sinks.end());

    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
    logger->set_level(level);
    logger->flush_on(spdlog::level::info);

    spdlog::set_default_logger(logger);
    spdlog::set_level(level);
    g_initialized = true;

    spdlog::info("Logger initialized: {}", fileName.str());
}

void ChessLog::shutdown() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_initialized) return;
    spdlog::info("Logger shutting down");
    spdlog::shutdown();
    g_initialized = false;
}

bool ChessLog::isInitialized() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    return g_initialized;
}
