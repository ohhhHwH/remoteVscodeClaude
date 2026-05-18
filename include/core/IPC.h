#pragma once
#include <string>
#include <windows.h>

class IPC {
public:
    bool CreatePipeServer(const std::string& pipeName);
    bool ConnectToPipe(const std::string& pipeName);
    bool Send(const std::string& data);
    std::string Receive();
    void Close();

private:
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
};
