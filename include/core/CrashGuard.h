#pragma once
#define NOMINMAX
#include <windows.h>
#include <string>
#include <functional>

// Forward-declare: called by CrashGuard to get recent logs before crash
using CrashLogProvider = std::function<std::string()>;

class CrashGuard {
public:
    // Install all crash handlers. Must be called once at startup.
    // logProvider: called on crash to collect recent log entries
    // dumpDir: where to save .dmp and crash_report.txt (default "logs")
    static void Install(const CrashLogProvider& logProvider = nullptr,
                        const std::string& dumpDir = "logs");

    // Set or update log provider after Install (e.g. after Logger is created)
    static void SetLogProvider(const CrashLogProvider& logProvider);

    // Get dump directory path
    static const std::string& GetDumpDir() { return dumpDir_; }

    // Test: generate a test crash report without crashing
    static void TestCrash();

private:
    // SEH unhandled exception filter
    static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* ep);

    // CRT abort() handler (C++ signal)
    static void __cdecl AbortHandler(int signal);

    // Pure virtual call handler
    static void __cdecl PureCallHandler();

    // Invalid parameter handler
    static void __cdecl InvalidParameterHandler(
        const wchar_t* expression,
        const wchar_t* function,
        const wchar_t* file,
        unsigned int line,
        uintptr_t reserved);

    // Write crash artifacts: minidump + crash report text file
    static void WriteCrashDump(EXCEPTION_POINTERS* ep, const std::string& reason);
    static void WriteMiniDump(EXCEPTION_POINTERS* ep, const std::string& path);
    static void WriteCrashReport(const std::string& path, EXCEPTION_POINTERS* ep,
                                  const std::string& reason);

    static CrashLogProvider logProvider_;
    static std::string dumpDir_;
};
