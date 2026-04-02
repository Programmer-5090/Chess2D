#ifndef PERF_PROFILER_H
#define PERF_PROFILER_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "logger.h"

// Lightweight profiler that accumulates named timer durations (microseconds).
// Usage: g_profiler.startTimer("name"); ... g_profiler.endTimer("name");

class PerformanceProfiler {
private:
    using Clock = std::chrono::high_resolution_clock;
    using Microseconds = std::chrono::microseconds;

    struct Frame {
        std::string label;
        Clock::time_point startTime;
        long long childMicros = 0; // accumulated inclusive time of child timers
        bool isRoot = false;
    };

    // Active timer stack (LIFO) kept per-thread to avoid cross-thread mismatches
    static thread_local std::vector<Frame> s_threadFrameStack;

    // Aggregate protection
    mutable std::mutex totalsMutex;

    // Aggregated totals
    std::unordered_map<std::string, long long> totalInclusiveMicros; // total inclusive time per label
    std::unordered_map<std::string, long long> totalExclusiveMicros; // total exclusive (self) time per label
    // parent -> (child -> inclusive)
    std::unordered_map<std::string, std::unordered_map<std::string, long long>> childInclusiveMicros;
    // parent -> (child -> count)
    std::unordered_map<std::string, std::unordered_map<std::string, long long>> childCallCounts;
    // Totals for timers that were started as root (stack depth 0)
    std::unordered_map<std::string, long long> rootInclusiveMicros;
    std::unordered_map<std::string, long long> rootCallCounts;
    std::unordered_map<std::string, long long> totalCallCounts;
    bool verbose = false; // emit per-call timing when true

public:
    void startTimer(const std::string& label);
    void endTimer(const std::string& label);
    // Enable/disable profiler (start/end become no-ops when disabled)
    void setEnabled(bool e);
    bool isEnabled() const;
    // Log aggregated report via the project's Logger (INFO level)
    void report() const;
    // Write aggregated report to a dedicated profile log file
    void writeReportToFile() const;
    // Set output directory for profiler reports (e.g., "profilelog")
    void setOutputDirectory(const std::string& dir);
    std::string getOutputDirectory() const;
    // Enable/disable per-call logging
    void setVerbose(bool v) { verbose = v; }
    bool isVerbose() const { return verbose; }
    // Clear all accumulated data
    void clear();

    // Detailed item for reporting
    struct DetailedItem {
        std::string name;
        long long inclusiveMicros = 0;
        long long exclusiveMicros = 0;
        long long callCount = 0;
        // Totals when this timer was started as a root (stack depth 0)
        long long rootInclusiveMicros = 0;
        long long rootCallCount = 0;
    };

    // Return sorted detailed items (by inclusive time desc)
    std::vector<DetailedItem> getDetailedItems() const;

    // Return top child contributors for a given parent
    struct ChildItem { std::string name; long long inclusiveMicros; long long callCount; };
    std::vector<ChildItem> getChildItemsDetailed(const std::string& parent) const;
    // Return totals for timers that were started at stack depth 0
    std::vector<std::pair<std::string, long long>> getRootItems() const;

    // Backwards-compatible: return inclusive sorted pairs
    std::vector<std::pair<std::string, long long>> getSortedItems() const;

private:
    std::string buildReportString() const;
    std::string outputDirectory = "profilelog"; // relative output folder for profiler reports
};

// Global profiler instance
extern PerformanceProfiler g_profiler;
// Global atomic flag controlling whether profiler timers are active
extern std::atomic<bool> g_profilerEnabled;

// RAII helper for scoping measurements.
// Usage: { ScopedTimer t("my op"); /* work */ }
struct ScopedTimer {
    std::string name;
    explicit ScopedTimer(const std::string& n) : name(n) {
        // Start a named timer on construction
        g_profiler.startTimer(name);
    }

    ~ScopedTimer() {
        // End the named timer on destruction (accumulates into profiler)
        g_profiler.endTimer(name);
    }
};

#endif // PERF_PROFILER_H