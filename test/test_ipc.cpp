#include "TestFramework.h"
#include "core/IPC.h"
#include <thread>
#include <chrono>

TEST(IPC_CreateAndConnect) {
    std::string pipeName = "test_pipe_basic";
    bool serverReady = false;
    bool clientConnected = false;
    std::string received;

    std::thread server([&]() {
        IPC ipc;
        serverReady = ipc.CreatePipeServer(pipeName);
        if (serverReady) {
            received = ipc.Receive();
            ipc.Close();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::thread client([&]() {
        IPC ipc;
        clientConnected = ipc.ConnectToPipe(pipeName);
        if (clientConnected) {
            ipc.Send("hello pipe");
            ipc.Close();
        }
    });

    server.join();
    client.join();

    ASSERT_TRUE(serverReady);
    ASSERT_TRUE(clientConnected);
    ASSERT_EQ(received, "hello pipe");
    return true;
}

TEST(IPC_BidirectionalComm) {
    std::string pipeName = "test_pipe_bidir";
    std::string serverReceived, clientReceived;

    std::thread server([&]() {
        IPC ipc;
        if (ipc.CreatePipeServer(pipeName)) {
            serverReceived = ipc.Receive();
            ipc.Send("pong");
            ipc.Close();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::thread client([&]() {
        IPC ipc;
        if (ipc.ConnectToPipe(pipeName)) {
            ipc.Send("ping");
            clientReceived = ipc.Receive();
            ipc.Close();
        }
    });

    server.join();
    client.join();

    ASSERT_EQ(serverReceived, "ping");
    ASSERT_EQ(clientReceived, "pong");
    return true;
}

TEST(IPC_ConnectNonexistent) {
    IPC ipc;
    ASSERT_FALSE(ipc.ConnectToPipe("nonexistent_pipe_xyz"));
    return true;
}

int main() {
    return RunAllTests();
}
