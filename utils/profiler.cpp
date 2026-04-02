#include <profiler.h>
#include <logger.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <mutex>

PerformanceProfiler g_profiler;
std::atomic<bool> g_profilerEnabled{true};
thread_local std::vector<PerformanceProfiler::Frame> PerformanceProfiler::s_threadFrameStack;

void PerformanceProfiler::setEnabled(bool enabled) { g_profilerEnabled.store(enabled, std::memory_order_relaxed); }

bool PerformanceProfiler::isEnabled() const {
    return g_profilerEnabled.load(std::memory_order_relaxed);
}

void PerformanceProfiler::startTimer(const std::string& label) {
    if (!isEnabled()) return;
    Frame frame;
    frame.label = label;
    frame.startTime = Clock::now();
    frame.childMicros = 0;
    frame.isRoot = s_threadFrameStack.empty();
    s_threadFrameStack.push_back(std::move(frame));
}

void PerformanceProfiler::endTimer(const std::string& label) {
    if (!isEnabled()) return;
    const auto endTime = Clock::now();

    if (s_threadFrameStack.empty()) return; // mismatched endTimer

    Frame frame = s_threadFrameStack.back();
    s_threadFrameStack.pop_back();

    if (frame.label != label) {
        std::ostringstream warnoss;
        warnoss << "PerformanceProfiler: timer mismatch. Expected '" << frame.label << "' got '" << label << "'";
        Logger::log(LogLevel::WARN, warnoss.str(), __FILE__, __LINE__);
    }

    const long long elapsedMicros = std::chrono::duration_cast<Microseconds>(endTime - frame.startTime).count();

    // inclusive adds entire elapsed time
    totalInclusiveMicros[frame.label] += elapsedMicros;
    // exclusive is elapsed minus time spent in child timers
    long long selfMicros = elapsedMicros - frame.childMicros;
    if (selfMicros < 0) selfMicros = 0; // safety
    totalExclusiveMicros[frame.label] += selfMicros;
    totalCallCounts[frame.label] += 1;

    const bool hasParent = !s_threadFrameStack.empty();
    const std::string parentLabel = hasParent ? s_threadFrameStack.back().label : std::string();

    // If there's a parent frame on the stack, add this elapsed to its childMicros
    if (hasParent) {
        s_threadFrameStack.back().childMicros += elapsedMicros;
    }

    // Aggregate updates are shared; protect with mutex
    std::lock_guard<std::mutex> lock(totalsMutex);

    // If this frame was started as a root, record root totals
    if (frame.isRoot) {
        rootInclusiveMicros[frame.label] += elapsedMicros;
        rootCallCounts[frame.label] += 1;
    }

    // Record parent->child inclusive mapping if there was a parent
    if (hasParent) {
        childInclusiveMicros[parentLabel][frame.label] += elapsedMicros;
        childCallCounts[parentLabel][frame.label] += 1;
    }

    // Emit per-call timing when verbose
    if (g_profiler.isVerbose()) {
        std::ostringstream oss;
        oss << "[PerformanceProfiler] " << frame.label << ": " << (elapsedMicros / 1000.0) << " ms (self=" << (selfMicros / 1000.0) << " ms)";
        Logger::log(LogLevel::DEBUG, oss.str(), __FILE__, __LINE__);
    }
}

void PerformanceProfiler::report() const {
    Logger::log(LogLevel::INFO, buildReportString(), __FILE__, __LINE__);
}

void PerformanceProfiler::writeReportToFile() const {
    const std::string report = buildReportString();

    std::filesystem::path outDir = outputDirectory.empty() ? std::filesystem::path("profilelog")
                                                           : std::filesystem::path(outputDirectory);
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream fname;
    fname << "profile_"
          << (tm.tm_year + 1900)
          << std::setw(2) << std::setfill('0') << (tm.tm_mon + 1)
          << std::setw(2) << std::setfill('0') << tm.tm_mday
          << "_"
          << std::setw(2) << std::setfill('0') << tm.tm_hour
          << std::setw(2) << std::setfill('0') << tm.tm_min
          << std::setw(2) << std::setfill('0') << tm.tm_sec
          << "_" << std::setw(3) << std::setfill('0') << ms.count()
          << ".log";

    std::filesystem::path outPath = outDir / fname.str();

    std::ofstream ofs(outPath.string(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        Logger::log(LogLevel::ERROR, "PerformanceProfiler: failed to open profile log file at " + outPath.string(), __FILE__, __LINE__);
        return;
    }

    ofs << report;
    ofs.flush();
    Logger::log(LogLevel::INFO, "PerformanceProfiler wrote profile report to " + outPath.string(), __FILE__, __LINE__);
}

void PerformanceProfiler::setOutputDirectory(const std::string& dir) {
    if (dir.empty()) return;
    outputDirectory = dir;
}

std::string PerformanceProfiler::getOutputDirectory() const {
    return outputDirectory;
}

std::string PerformanceProfiler::buildReportString() const {
    std::ostringstream oss;
    oss << "\n=== Performance Profiler Report ===\n";

    // Build detailed items
    std::vector<DetailedItem> items;
    {
        std::lock_guard<std::mutex> lock(totalsMutex);
        for (const auto &kv : totalInclusiveMicros) {
            DetailedItem it;
            it.name = kv.first;
            it.inclusiveMicros = kv.second;
            auto exIt = totalExclusiveMicros.find(kv.first);
            if (exIt != totalExclusiveMicros.end()) it.exclusiveMicros = exIt->second;
            auto cntIt = totalCallCounts.find(kv.first);
            if (cntIt != totalCallCounts.end()) it.callCount = cntIt->second;
            auto rootInclIt = rootInclusiveMicros.find(kv.first);
            if (rootInclIt != rootInclusiveMicros.end()) it.rootInclusiveMicros = rootInclIt->second;
            auto rootCntIt = rootCallCounts.find(kv.first);
            if (rootCntIt != rootCallCounts.end()) it.rootCallCount = rootCntIt->second;
            items.push_back(it);
        }
    }

    // sort by inclusive time desc
    std::sort(items.begin(), items.end(), [](const DetailedItem &a, const DetailedItem &b) {
        return a.inclusiveMicros > b.inclusiveMicros;
    });

    for (const auto &p : items) {
        double incl_ms = p.inclusiveMicros / 1000.0;
        double excl_ms = p.exclusiveMicros / 1000.0;
        double avg_ms = p.callCount ? (p.inclusiveMicros / 1000.0 / p.callCount) : 0.0;
        oss << p.name << ": incl=" << incl_ms << " ms, excl=" << excl_ms << " ms, calls=" << p.callCount << ", avg(incl)=" << avg_ms << " ms\n";
    }

    oss << "=== End Performance Report ===\n\n";

    oss << "=== End Performance Report ===\n\n";

    return oss.str();
}

std::vector<PerformanceProfiler::DetailedItem> PerformanceProfiler::getDetailedItems() const {
    std::vector<DetailedItem> items;
    {
        std::lock_guard<std::mutex> lock(totalsMutex);
        for (const auto &kv : totalInclusiveMicros) {
            DetailedItem it;
            it.name = kv.first;
            it.inclusiveMicros = kv.second;
            auto exIt = totalExclusiveMicros.find(kv.first);
            if (exIt != totalExclusiveMicros.end()) it.exclusiveMicros = exIt->second;
            auto cntIt = totalCallCounts.find(kv.first);
            if (cntIt != totalCallCounts.end()) it.callCount = cntIt->second;
            items.push_back(it);
        }
    }
    std::sort(items.begin(), items.end(), [](const DetailedItem &a, const DetailedItem &b) {
        return a.inclusiveMicros > b.inclusiveMicros;
    });
    return items;
}

std::vector<std::pair<std::string, long long>> PerformanceProfiler::getSortedItems() const {
    std::vector<std::pair<std::string, long long>> items;
    {
        std::lock_guard<std::mutex> lock(totalsMutex);
        for (const auto& kv : totalInclusiveMicros) items.emplace_back(kv.first, kv.second);
    }
    std::sort(items.begin(), items.end(), [](auto &a, auto &b){ return a.second > b.second; });
    return items;
}

std::vector<PerformanceProfiler::ChildItem> PerformanceProfiler::getChildItemsDetailed(const std::string& parent) const {
    std::vector<ChildItem> items;
    {
        std::lock_guard<std::mutex> lock(totalsMutex);
        auto it = childInclusiveMicros.find(parent);
        if (it == childInclusiveMicros.end()) return items;
        for (const auto &kv : it->second) {
            ChildItem ci;
            ci.name = kv.first;
            ci.inclusiveMicros = kv.second;
            auto ccIt = childCallCounts.find(parent);
            if (ccIt != childCallCounts.end()) {
                auto cIt2 = ccIt->second.find(kv.first);
                if (cIt2 != ccIt->second.end()) ci.callCount = cIt2->second;
            }
            items.push_back(ci);
        }
    }
    std::sort(items.begin(), items.end(), [](const ChildItem &a, const ChildItem &b){ return a.inclusiveMicros > b.inclusiveMicros; });
    return items;
}

std::vector<std::pair<std::string, long long>> PerformanceProfiler::getRootItems() const {
    std::vector<std::pair<std::string, long long>> items;
    {
        std::lock_guard<std::mutex> lock(totalsMutex);
        for (const auto &kv : rootInclusiveMicros) items.emplace_back(kv.first, kv.second);
    }
    std::sort(items.begin(), items.end(), [](auto &a, auto &b){ return a.second > b.second; });
    return items;
}

void PerformanceProfiler::clear() {
    std::lock_guard<std::mutex> lock(totalsMutex);
    totalInclusiveMicros.clear();
    totalExclusiveMicros.clear();
    childInclusiveMicros.clear();
    childCallCounts.clear();
    rootInclusiveMicros.clear();
    rootCallCounts.clear();
    totalCallCounts.clear();
}
