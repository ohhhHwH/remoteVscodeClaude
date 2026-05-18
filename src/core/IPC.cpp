#include "core/IPC.h"

bool IPC::CreatePipeServer(const std::string& pipeName) {
    std::string fullName = "\\\\.\\pipe\\" + pipeName;
    pipe_ = CreateNamedPipeA(
        fullName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 4096, 4096, 0, nullptr
    );
    if (pipe_ == INVALID_HANDLE_VALUE) return false;
    ConnectNamedPipe(pipe_, nullptr);
    return true;
}

bool IPC::ConnectToPipe(const std::string& pipeName) {
    std::string fullName = "\\\\.\\pipe\\" + pipeName;
    pipe_ = CreateFileA(
        fullName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr
    );
    return pipe_ != INVALID_HANDLE_VALUE;
}

bool IPC::Send(const std::string& data) {
    DWORD written;
    return WriteFile(pipe_, data.c_str(), (DWORD)data.size(), &written, nullptr);
}

std::string IPC::Receive() {
    char buffer[4096];
    DWORD bytesRead;
    if (ReadFile(pipe_, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
        buffer[bytesRead] = '\0';
        return std::string(buffer, bytesRead);
    }
    return "";
}

void IPC::Close() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}
