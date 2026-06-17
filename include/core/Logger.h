#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <fstream>
#include <ctime>

// Windows wingdi.h defines ERROR as 0, which breaks our enum
#ifdef ERROR
#undef ERROR
#endif
#ifdef DEBUG
#undef DEBUG
#endif

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

struct LogEntry {
    LogLevel level;
    std::string category;
    std::string message;
    std::string formatted;  // full formatted line
    time_t timestamp;
};

class Logger {
public:
    Logger() = default;
    ~Logger();

    // Initialize: set log directory, minimum level, max file size (MB)
    bool Init(const std::string& logDir, LogLevel level = LogLevel::Info, int maxSizeMB = 10);

    // Core log method — thread-safe
    void Log(LogLevel level, const std::string& category, const std::string& message);

    // Convenience methods
    void Debug(const std::string& category, const std::string& msg) { Log(LogLevel::Debug, category, msg); }
    void Info(const std::string& category, const std::string& msg)  { Log(LogLevel::Info, category, msg); }
    void Warn(const std::string& category, const std::string& msg)  { Log(LogLevel::Warn, category, msg); }
    void Error(const std::string& category, const std::string& msg) { Log(LogLevel::Error, category, msg); }

    // In-memory access for GUI display
    std::vector<LogEntry> GetRecent(int count = 50) const;
    std::vector<LogEntry> GetByCategory(const std::string& category, int count = 50) const;
    std::vector<LogEntry> GetByLevel(LogLevel minLevel, int count = 50) const;

    // Configuration
    void SetLevel(LogLevel level);
    LogLevel GetLevel() const { return minLevel_; }
    bool IsEnabled() const { return initialized_; }
    const std::string& GetLogDir() const { return logDir_; }

    // Ensure pending writes are flushed
    void Flush();

private:
    std::string FormatEntry(const LogEntry& entry) const;
    std::string TimeStr(time_t t) const;
    void OpenLogFile();
    void CheckRotation();
    void WriteToFile(const std::string& line);
    std::string TodayStr() const;

    mutable std::mutex mutex_;
    std::string logDir_;
    std::string currentLogPath_;
    std::ofstream logFile_;
    LogLevel minLevel_ = LogLevel::Info;
    int maxSizeMB_ = 10;
    bool initialized_ = false;

    // In-memory ring buffer (keep last N entries for GUI)
    static constexpr size_t kMaxEntries = 2000;
    std::deque<LogEntry> entries_;
};
