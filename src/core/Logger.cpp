#include "core/Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

Logger::~Logger() {
    Flush();
}

bool Logger::Init(const std::string& logDir, LogLevel level, int maxSizeMB) {
    std::lock_guard<std::mutex> lk(mutex_);
    logDir_ = logDir;
    minLevel_ = level;
    maxSizeMB_ = maxSizeMB;

    // Create log directory if it doesn't exist
    try {
        std::filesystem::create_directories(logDir_);
    } catch (...) {
        return false;
    }

    OpenLogFile();
    initialized_ = logFile_.is_open();
    return initialized_;
}

void Logger::Log(LogLevel level, const std::string& category, const std::string& message) {
    if (level < minLevel_) return;

    LogEntry entry;
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.timestamp = std::time(nullptr);
    entry.formatted = FormatEntry(entry);

    {
        std::lock_guard<std::mutex> lk(mutex_);
        // In-memory ring buffer
        entries_.push_back(entry);
        if (entries_.size() > kMaxEntries) {
            entries_.pop_front();
        }

        // File output
        CheckRotation();
        if (logFile_.is_open()) {
            logFile_ << entry.formatted << std::endl;
        }
    }
}

std::vector<LogEntry> Logger::GetRecent(int count) const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<LogEntry> result;
    if (entries_.empty()) return result;
    if (count <= 0) count = 50;
    size_t start = entries_.size() > (size_t)count ? entries_.size() - count : 0;
    for (size_t i = start; i < entries_.size(); i++) {
        result.push_back(entries_[i]);
    }
    return result;
}

std::vector<LogEntry> Logger::GetByCategory(const std::string& category, int count) const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<LogEntry> result;
    // Iterate in reverse to get most recent first
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->category == category) {
            result.push_back(*it);
            if (count > 0 && (int)result.size() >= count) break;
        }
    }
    return result;
}

std::vector<LogEntry> Logger::GetByLevel(LogLevel minLevel, int count) const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<LogEntry> result;
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->level >= minLevel) {
            result.push_back(*it);
            if (count > 0 && (int)result.size() >= count) break;
        }
    }
    return result;
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lk(mutex_);
    minLevel_ = level;
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (logFile_.is_open()) {
        logFile_.flush();
    }
}

std::string Logger::FormatEntry(const LogEntry& entry) const {
    // [YYYY-MM-DD HH:MM:SS] [LEVEL] [Category] message
    const char* levelNames[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char* levelStr = levelNames[(int)entry.level];
    if ((int)entry.level > 3) levelStr = "UNKNOWN";

    return "[" + TimeStr(entry.timestamp) + "] [" + levelStr + "] [" +
           entry.category + "] " + entry.message;
}

std::string Logger::TimeStr(time_t t) const {
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

std::string Logger::TodayStr() const {
    time_t now = std::time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y%m%d", &tm_buf);
    return buf;
}

void Logger::OpenLogFile() {
    currentLogPath_ = logDir_ + "/RemoteMonitor_" + TodayStr() + ".log";
    logFile_.open(currentLogPath_, std::ios::app);
}

void Logger::CheckRotation() {
    std::string todayPath = logDir_ + "/RemoteMonitor_" + TodayStr() + ".log";

    // Day changed — open new file
    if (todayPath != currentLogPath_) {
        logFile_.close();
        OpenLogFile();
        return;
    }

    // Size check — if over maxSizeMB, add sequence suffix
    if (logFile_.is_open() && maxSizeMB_ > 0) {
        long pos = (long)logFile_.tellp();
        if (pos > (long)(maxSizeMB_ * 1024 * 1024)) {
            logFile_.close();
            // Rename current file with sequence
            std::string base = logDir_ + "/RemoteMonitor_" + TodayStr();
            int seq = 1;
            std::string newPath;
            do {
                newPath = base + "_" + std::to_string(seq) + ".log";
                seq++;
            } while (std::filesystem::exists(newPath));
            try {
                std::filesystem::rename(currentLogPath_, newPath);
            } catch (...) {}
            // Reopen main log file
            OpenLogFile();
        }
    }
}

void Logger::WriteToFile(const std::string& line) {
    if (logFile_.is_open()) {
        logFile_ << line << std::endl;
    }
}
