#include <logger.h>
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define SAFE_LOCALTIME(timer, buf) localtime_s(buf, timer)
#else
#include <sys/stat.h>
#include <unistd.h>
#define SAFE_LOCALTIME(timer, buf) localtime_r(timer, buf)
#endif

std::ofstream Logger::s_logStream;
std::mutex Logger::s_logMutex;
bool Logger::s_isInitialized = false;
std::streambuf* Logger::s_prevCerrBuf = nullptr;
std::streambuf* Logger::s_prevCoutBuf = nullptr;
std::string Logger::s_activeLogPath;
LogLevel Logger::s_minLogLevel = LogLevel::INFO;
size_t Logger::s_maxFileBytes = 50 * 1024 * 1024; // 50MB default
bool Logger::s_redirectStdIO = true;
bool Logger::s_isSilent = false;
bool Logger::s_useConsoleColors = true;

void Logger::init(const std::string& logDir, LogLevel minLevel, bool redirectStreams, size_t maxFileSizeMB) {
    bool didInit = false;
    try {
        std::lock_guard<std::mutex> lock(s_logMutex);
        if (s_isInitialized) {
            std::cerr << "Logger already initialized" << std::endl;
            return;
        }

        s_isSilent = false;
        s_minLogLevel = minLevel;
        s_redirectStdIO = redirectStreams;
        s_maxFileBytes = maxFileSizeMB * 1024 * 1024;

        std::filesystem::create_directories(logDir);

        // Create filename with timestamp and thread id
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::tm tm;
        SAFE_LOCALTIME(&t, &tm);

        auto pid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        std::ostringstream filename;
        filename << logDir << "/log_"
                 << (tm.tm_year + 1900)
                 << std::setw(2) << std::setfill('0') << (tm.tm_mon + 1)
                 << std::setw(2) << std::setfill('0') << tm.tm_mday
                 << "_"
                 << std::setw(2) << std::setfill('0') << tm.tm_hour
                 << std::setw(2) << std::setfill('0') << tm.tm_min
                 << std::setw(2) << std::setfill('0') << tm.tm_sec
                 << "_" << std::setw(3) << std::setfill('0') << ms.count()
                 << "_" << std::hex << (pid & 0xFFFF) << ".log";

        s_activeLogPath = filename.str();
        s_logStream.open(s_activeLogPath, std::ios::app);
        if (!s_logStream.is_open()) {
            std::cerr << "Logger: Failed to open log file " << s_activeLogPath << std::endl;
            return;
        }

        writeSessionHeader();

        if (s_redirectStdIO) {
            s_prevCerrBuf = std::cerr.rdbuf();
            s_prevCoutBuf = std::cout.rdbuf();
            std::cerr.rdbuf(s_logStream.rdbuf());
            std::cout.rdbuf(s_logStream.rdbuf());
        }

        s_isInitialized = true;
        didInit = true;
    } catch (const std::exception& e) {
        std::cerr << "Logger init exception: " << e.what() << std::endl;
    }

    if (didInit) {
        log(LogLevel::INFO, "Logger initialized successfully. Log file: " + s_activeLogPath, __FILE__, __LINE__);
    }
}

void Logger::shutdown() {
    // Mark uninitialized first so subsequent log() uses stderr
    {
        std::lock_guard<std::mutex> lock(s_logMutex);
        if (!s_isInitialized) return;
        s_isInitialized = false;
    }

    log(LogLevel::INFO, "Logger shutting down", __FILE__, __LINE__);

    std::lock_guard<std::mutex> lock2(s_logMutex);
    if (s_logStream.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        SAFE_LOCALTIME(&t, &tm);
        s_logStream << std::endl << "=== Logger shutdown at " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ===" << std::endl << std::endl;
        s_logStream.flush();

        if (s_redirectStdIO) {
            if (s_prevCerrBuf) std::cerr.rdbuf(s_prevCerrBuf);
            if (s_prevCoutBuf) std::cout.rdbuf(s_prevCoutBuf);
        }
        s_logStream.close();
    }

    // Prevent logging from future destructors
    s_isSilent = true;
    s_activeLogPath.clear();
    s_minLogLevel = LogLevel::INFO;
}

void Logger::log(LogLevel level, const std::string& msg, const char* file, int line) {
    std::lock_guard<std::mutex> lock(s_logMutex);

    // Silent mode
    if (s_isSilent) {
        return;
    }

    // Check if we should log this level
    if (level < s_minLogLevel) {
        return;
    }
    
    // Check file size and rotate if necessary
    if (s_isInitialized && s_logStream.is_open()) {
        rotateLogIfNeeded();
    }
    
    const std::string timestamp = buildTimestamp();

    // Level string and color
    const char* levelStr = getLevelString(level);
    const char* colorCode = getColorCode(level);
    
    // Extract just the filename from the full path
    std::string filename = extractFilename(file);
    
    // Format the complete log message
    std::ostringstream logMessage;
    logMessage << timestamp << " [" << levelStr << "] " 
               << msg << " (" << filename << ":" << line << ")";
    
    // Write to file if initialized
    if (s_isInitialized && s_logStream.is_open()) {
        s_logStream << logMessage.str() << std::endl;
        s_logStream.flush();
    } else {
        writeConsole(logMessage.str(), colorCode);
    }
}

void Logger::setMinLevel(LogLevel level) {
    {
        std::lock_guard<std::mutex> lock(s_logMutex);
        s_minLogLevel = level;
    }
    std::string msg = "Log level changed to ";
    msg += getLevelString(level);
    log(LogLevel::INFO, msg, __FILE__, __LINE__);
}

void Logger::setSilent(bool silent) {
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_isSilent = silent;
}

void Logger::setConsoleColors(bool enabled) {
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_useConsoleColors = enabled;
}

bool Logger::consoleColorsEnabled() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    return s_useConsoleColors;
}

bool Logger::isSilent() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    return s_isSilent;
}

LogLevel Logger::getMinLevel() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    return s_minLogLevel;
}

std::string Logger::getCurrentLogFile() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    return s_activeLogPath;
}

bool Logger::isInitialized() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    return s_isInitialized;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (s_isInitialized && s_logStream.is_open()) {
        s_logStream.flush();
    }
}

const char* Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKN ";
    }
}

const char* Logger::getColorCode(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "\033[36m"; // Cyan
        case LogLevel::INFO:  return "\033[32m"; // Green
        case LogLevel::WARN:  return "\033[33m"; // Yellow
        case LogLevel::ERROR: return "\033[31m"; // Red
        default:              return "\033[0m";  // Reset
    }
}

std::string Logger::extractFilename(const char* path) {
    std::string fullPath(path);
    size_t pos = fullPath.find_last_of("/\\");
    if (pos != std::string::npos) {
        return fullPath.substr(pos + 1);
    }
    return fullPath;
}

void Logger::writeSessionHeader() {
    if (!s_logStream.is_open()) return;
    
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    SAFE_LOCALTIME(&t, &tm);
    
    s_logStream << "=== Logger started at " 
                << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") 
                << " ===" << std::endl;
    s_logStream << "Log file: " << s_activeLogPath << std::endl;
    s_logStream << "Min level: " << getLevelString(s_minLogLevel) << std::endl;
    s_logStream << "Max file size: " << (s_maxFileBytes / 1024 / 1024) << " MB" << std::endl;
    s_logStream << "Stream redirection: " << (s_redirectStdIO ? "enabled" : "disabled") << std::endl;
    s_logStream << "========================================" << std::endl << std::endl;
    s_logStream.flush();
}

void Logger::rotateLogIfNeeded() {
    if (!s_logStream.is_open()) return;
    
    try {
        // Get current file size
        s_logStream.flush();
        std::filesystem::path logPath(s_activeLogPath);
        
        if (std::filesystem::exists(logPath)) {
            size_t fileSize = std::filesystem::file_size(logPath);
            
            if (fileSize >= s_maxFileBytes) {
                // Close current file
                s_logStream.close();
                
                // Create new filename with rotation suffix
                std::string baseName = s_activeLogPath.substr(0, s_activeLogPath.find_last_of('.'));
                std::string extension = s_activeLogPath.substr(s_activeLogPath.find_last_of('.'));
                
                // Find next available rotation number
                int rotationNum = 1;
                std::string rotatedName;
                do {
                    std::ostringstream oss;
                    oss << baseName << "_part" << std::setw(3) << std::setfill('0') << rotationNum << extension;
                    rotatedName = oss.str();
                    rotationNum++;
                } while (std::filesystem::exists(rotatedName));
                
                s_activeLogPath = rotatedName;
                s_logStream.open(s_activeLogPath, std::ios::app);
                
                if (s_logStream.is_open()) {
                    writeSessionHeader();
                    log(LogLevel::INFO, "Log rotated to new file: " + s_activeLogPath, __FILE__, __LINE__);
                }
            }
        }
    } catch (const std::exception& e) {
        // If rotation fails, continue with current file
        if (!s_logStream.is_open()) {
            s_logStream.open(s_activeLogPath, std::ios::app);
        }
    }
}

std::string Logger::buildTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    SAFE_LOCALTIME(&t, &tm);

    std::ostringstream timestamp;
    timestamp << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << "." << std::setw(3) << std::setfill('0') << ms.count();
    return timestamp.str();
}

void Logger::writeConsole(const std::string& message, const char* colorCode) {
    if (!s_useConsoleColors) {
        std::cerr << message << std::endl;
        return;
    }

    constexpr const char* resetColor = "\033[0m";
    std::cerr << colorCode << message << resetColor << std::endl;
}