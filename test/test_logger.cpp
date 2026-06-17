#include "TestFramework.h"
#include "core/Logger.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <algorithm>

// Clean up test log directory
static void CleanLogDir(const std::string& dir) {
    try {
        if (std::filesystem::exists(dir)) {
            std::filesystem::remove_all(dir);
        }
    } catch (...) {}
}

// Helper: read all lines from a log file
static std::vector<std::string> ReadLogLines(const std::string& dir) {
    std::vector<std::string> lines;
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".log") {
            std::ifstream f(entry.path().string());
            std::string l;
            while (std::getline(f, l)) lines.push_back(l);
        }
    }
    return lines;
}

TEST(logger_init) {
    CleanLogDir("test_logs_init");
    Logger logger;
    ASSERT_TRUE(logger.Init("test_logs_init", LogLevel::Debug, 1));
    ASSERT_TRUE(logger.IsEnabled());
    ASSERT_EQ(logger.GetLevel(), LogLevel::Debug);
    ASSERT_EQ(logger.GetLogDir(), "test_logs_init");
    CleanLogDir("test_logs_init");
    return true;
}

TEST(logger_write_and_read) {
    CleanLogDir("test_logs_rw");
    Logger logger;
    logger.Init("test_logs_rw", LogLevel::Debug);
    logger.Info("TestCat", "hello world");
    logger.Warn("Monitor", "something happened");
    logger.Error("System", "fatal error");

    auto recent = logger.GetRecent(10);
    ASSERT_EQ((int)recent.size(), 3);

    auto cat = logger.GetByCategory("TestCat", 10);
    ASSERT_EQ((int)cat.size(), 1);
    ASSERT_TRUE(cat[0].message.find("hello world") != std::string::npos);

    auto cat2 = logger.GetByCategory("Monitor", 10);
    ASSERT_EQ((int)cat2.size(), 1);

    // Check file output
    auto lines = ReadLogLines("test_logs_rw");
    ASSERT_TRUE(lines.size() >= 3);

    CleanLogDir("test_logs_rw");
    return true;
}

TEST(logger_level_filtering) {
    CleanLogDir("test_logs_level");
    Logger logger;
    // Only WARN and above
    logger.Init("test_logs_level", LogLevel::Warn);
    logger.Debug("Cat", "debug msg");
    logger.Info("Cat", "info msg");
    logger.Warn("Cat", "warn msg");
    logger.Error("Cat", "error msg");

    auto recent = logger.GetRecent(10);
    ASSERT_EQ((int)recent.size(), 2);  // only WARN and ERROR

    auto filtered = logger.GetByLevel(LogLevel::Error, 10);
    ASSERT_EQ((int)filtered.size(), 1);

    CleanLogDir("test_logs_level");
    return true;
}

TEST(logger_category_filtering) {
    CleanLogDir("test_logs_cat");
    Logger logger;
    logger.Init("test_logs_cat", LogLevel::Debug);
    logger.Info("Monitor", "mon-1");
    logger.Info("Comm", "comm-1");
    logger.Info("Action", "act-1");
    logger.Info("Monitor", "mon-2");
    logger.Info("Comm", "comm-2");

    auto mon = logger.GetByCategory("Monitor", 10);
    ASSERT_EQ((int)mon.size(), 2);

    auto comm = logger.GetByCategory("Comm", 10);
    ASSERT_EQ((int)comm.size(), 2);

    auto act = logger.GetByCategory("Action", 10);
    ASSERT_EQ((int)act.size(), 1);

    auto none = logger.GetByCategory("Nonexistent", 10);
    ASSERT_EQ((int)none.size(), 0);

    CleanLogDir("test_logs_cat");
    return true;
}

TEST(logger_ring_buffer) {
    CleanLogDir("test_logs_ring");
    Logger logger;
    logger.Init("test_logs_ring", LogLevel::Debug);

    // Write 100 entries — check only recent 50 are returned by GetRecent(50)
    for (int i = 0; i < 100; i++) {
        logger.Info("Cat", "msg_" + std::to_string(i));
    }

    auto recent = logger.GetRecent(50);
    ASSERT_EQ((int)recent.size(), 50);
    // Should contain the last 50 messages
    ASSERT_TRUE(recent[0].message.find("msg_50") != std::string::npos);
    ASSERT_TRUE(recent[49].message.find("msg_99") != std::string::npos);

    CleanLogDir("test_logs_ring");
    return true;
}

TEST(logger_thread_safety) {
    CleanLogDir("test_logs_thread");
    Logger logger;
    logger.Init("test_logs_thread", LogLevel::Debug);

    const int kThreads = 4;
    const int kMsgsPerThread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([&logger, t, kMsgsPerThread]() {
            for (int i = 0; i < kMsgsPerThread; i++) {
                logger.Info("Thread_" + std::to_string(t), "msg_" + std::to_string(i));
            }
        });
    }

    for (auto& th : threads) th.join();

    auto recent = logger.GetRecent(2000);
    ASSERT_TRUE((int)recent.size() >= kThreads * kMsgsPerThread - 5);  // allow small race

    CleanLogDir("test_logs_thread");
    return true;
}

TEST(logger_formatted_entry) {
    CleanLogDir("test_logs_fmt");
    Logger logger;
    logger.Init("test_logs_fmt", LogLevel::Debug);
    logger.Info("Cat", "test message");

    auto entries = logger.GetRecent(1);
    ASSERT_EQ((int)entries.size(), 1);
    auto& e = entries[0];
    // Check format: [YYYY-MM-DD HH:MM:SS] [INFO] [Cat] test message
    ASSERT_TRUE(e.formatted.find("[INFO]") != std::string::npos);
    ASSERT_TRUE(e.formatted.find("[Cat]") != std::string::npos);
    ASSERT_TRUE(e.formatted.find("test message") != std::string::npos);
    ASSERT_EQ(e.level, LogLevel::Info);
    ASSERT_EQ(e.category, "Cat");

    CleanLogDir("test_logs_fmt");
    return true;
}

TEST(logger_set_level) {
    CleanLogDir("test_logs_setlevel");
    Logger logger;
    logger.Init("test_logs_setlevel", LogLevel::Info);
    ASSERT_EQ(logger.GetLevel(), LogLevel::Info);

    logger.SetLevel(LogLevel::Error);
    ASSERT_EQ(logger.GetLevel(), LogLevel::Error);

    logger.Info("Test", "should be filtered");
    logger.Error("Test", "should appear");

    auto recent = logger.GetRecent(10);
    ASSERT_EQ((int)recent.size(), 1);
    ASSERT_TRUE(recent[0].message.find("should appear") != std::string::npos);

    CleanLogDir("test_logs_setlevel");
    return true;
}

TEST(logger_flush_no_crash) {
    CleanLogDir("test_logs_flush");
    Logger logger;
    logger.Init("test_logs_flush");
    logger.Info("Cat", "before flush");
    logger.Flush();   // should not crash
    logger.Info("Cat", "after flush");
    logger.Flush();

    auto lines = ReadLogLines("test_logs_flush");
    ASSERT_TRUE(lines.size() >= 2);
    CleanLogDir("test_logs_flush");
    return true;
}

int main() {
    return RunAllTests();
}
