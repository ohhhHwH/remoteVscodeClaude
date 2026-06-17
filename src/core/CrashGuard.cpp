#include "core/CrashGuard.h"
#include <dbghelp.h>
#include <signal.h>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <sstream>
#include <cstdlib>
#include <new>

#pragma comment(lib, "dbghelp.lib")

CrashLogProvider CrashGuard::logProvider_ = nullptr;
std::string CrashGuard::dumpDir_ = "logs";

// --- Human-readable exception name ---
static const char* ExceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:       return "FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:             return "FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:          return "FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:            return "FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
        default: return "UNKNOWN_EXCEPTION";
    }
}

// --- Timestamp helpers ---
static std::string NowStr() {
    time_t t = std::time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_buf);
    return buf;
}

static std::string FullNowStr() {
    time_t t = std::time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

// --- Write crash report text ---
void CrashGuard::WriteCrashReport(const std::string& path, EXCEPTION_POINTERS* ep,
                                   const std::string& reason) {
    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "=== RemoteMonitor Crash Report ===\n";
    f << "Time:     " << FullNowStr() << "\n";
    f << "Reason:   " << reason << "\n";

    if (ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        f << "Exception: 0x" << std::hex << code << std::dec
          << " (" << ExceptionName(code) << ")\n";
        f << "Address:  0x" << std::hex
          << (uintptr_t)ep->ExceptionRecord->ExceptionAddress << std::dec << "\n";
        f << "ThreadID: " << GetCurrentThreadId() << "\n";
        f << "ProcessID: " << GetCurrentProcessId() << "\n";

        for (DWORD i = 0; i < ep->ExceptionRecord->NumberParameters; i++) {
            f << "Param[" << i << "]: 0x" << std::hex
              << ep->ExceptionRecord->ExceptionInformation[i] << std::dec << "\n";
        }

        if (ep->ContextRecord) {
            CONTEXT& ctx = *ep->ContextRecord;
#ifdef _WIN64
            f << "\n--- Registers (x64) ---\n";
            f << "Rax=0x" << std::hex << ctx.Rax << " Rbx=0x" << ctx.Rbx
              << " Rcx=0x" << ctx.Rcx << " Rdx=0x" << ctx.Rdx << "\n";
            f << "Rsi=0x" << ctx.Rsi << " Rdi=0x" << ctx.Rdi
              << " Rsp=0x" << ctx.Rsp << " Rbp=0x" << ctx.Rbp << "\n";
            f << "Rip=0x" << ctx.Rip << " R8=0x" << ctx.R8
              << " R9=0x" << ctx.R9 << " R10=0x" << ctx.R10 << "\n";
            f << "EFlags=0x" << ctx.EFlags << " SegCs=" << ctx.SegCs
              << " SegDs=" << ctx.SegDs << "\n" << std::dec;
#else
            f << "\n--- Registers (x86) ---\n";
            f << "Eax=0x" << std::hex << ctx.Eax << " Ebx=0x" << ctx.Ebx
              << " Ecx=0x" << ctx.Ecx << " Edx=0x" << ctx.Edx << "\n";
            f << "Esi=0x" << ctx.Esi << " Edi=0x" << ctx.Edi
              << " Esp=0x" << ctx.Esp << " Ebp=0x" << ctx.Ebp << "\n";
            f << "Eip=0x" << ctx.Eip << " EFlags=0x" << ctx.EFlags
              << " SegCs=" << ctx.SegCs << "\n" << std::dec;
#endif
        }
    }

    if (CrashGuard::logProvider_) {
        f << "\n--- Recent Logs ---\n";
        f << CrashGuard::logProvider_();
    }

    f << "\n--- End of Report ---\n";
    f.close();
}

// --- Write minidump ---
void CrashGuard::WriteMiniDump(EXCEPTION_POINTERS* ep, const std::string& path) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(
        MiniDumpNormal |
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules);

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                      dumpType, ep ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(hFile);
}

// ===== Write crash dump (orchestrator) =====
void CrashGuard::WriteCrashDump(EXCEPTION_POINTERS* ep, const std::string& reason) {
    std::string ts = NowStr();
    try {
        std::filesystem::create_directories(dumpDir_);
    } catch (...) {}

    std::string dumpPath = dumpDir_ + "/crash_" + ts + ".dmp";
    std::string reportPath = dumpDir_ + "/crash_" + ts + ".txt";

    WriteMiniDump(ep, dumpPath);
    WriteCrashReport(reportPath, ep, reason);
}

// ===== Install handlers =====

void CrashGuard::Install(const CrashLogProvider& logProvider, const std::string& dumpDir) {
    logProvider_ = logProvider;
    dumpDir_ = dumpDir;

    try { std::filesystem::create_directories(dumpDir_); } catch (...) {}

    SetUnhandledExceptionFilter(UnhandledExceptionFilter);
    signal(SIGABRT, AbortHandler);
    _set_purecall_handler(PureCallHandler);
    _set_invalid_parameter_handler(InvalidParameterHandler);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void CrashGuard::SetLogProvider(const CrashLogProvider& logProvider) {
    logProvider_ = logProvider;
}

// ===== Crash handlers =====

LONG WINAPI CrashGuard::UnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    WriteCrashDump(ep, "Unhandled SEH exception");
    return EXCEPTION_EXECUTE_HANDLER;
}

void __cdecl CrashGuard::AbortHandler(int /*signal*/) {
    WriteCrashDump(nullptr, "abort() called (CRT abort signal)");
    std::abort();
}

void __cdecl CrashGuard::PureCallHandler() {
    WriteCrashDump(nullptr, "Pure virtual function call");
    std::abort();
}

void __cdecl CrashGuard::InvalidParameterHandler(
    const wchar_t* expression,
    const wchar_t* function,
    const wchar_t* file,
    unsigned int line,
    uintptr_t /*reserved*/) {

    auto w2s = [](const wchar_t* w) -> std::string {
        if (!w) return "(null)";
        int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return "(conversion error)";
        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, -1, &result[0], len, nullptr, nullptr);
        return result;
    };

    std::string reason = "CRT invalid parameter\n"
        "  expression: " + w2s(expression) + "\n" +
        "  function:   " + w2s(function) + "\n" +
        "  file:       " + w2s(file) + "\n" +
        "  line:       " + std::to_string(line);

    WriteCrashDump(nullptr, reason);
}

// ===== Test =====

void CrashGuard::TestCrash() {
    WriteCrashReport(dumpDir_ + "/crash_test_" + NowStr() + ".txt",
                     nullptr, "Test crash report — no actual exception");
}
