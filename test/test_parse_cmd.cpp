#include "TestFramework.h"
#include <string>
#include <vector>
#include <windows.h>

struct Command {
    std::string action;
    std::string params;
};

static std::vector<Command> ParseCommands(const std::string& text) {
    std::vector<Command> cmds;
    size_t pos = 0;
    while ((pos = text.find("CMD:", pos)) != std::string::npos) {
        pos += 4;
        size_t end = text.find_first_of("\r\n", pos);
        std::string segment = (end != std::string::npos) ? text.substr(pos, end - pos) : text.substr(pos);

        size_t start = 0;
        while (start < segment.size()) {
            size_t semi = segment.find(';', start);
            std::string token = (semi != std::string::npos)
                ? segment.substr(start, semi - start)
                : segment.substr(start);
            start = (semi != std::string::npos) ? semi + 1 : segment.size();

            if (token.empty()) continue;

            Command cmd;
            size_t colon = token.find(':');
            if (colon != std::string::npos) {
                cmd.action = token.substr(0, colon);
                cmd.params = token.substr(colon + 1);
            } else {
                cmd.action = token;
            }
            while (!cmd.action.empty() && cmd.action.front() == ' ') cmd.action.erase(0, 1);
            while (!cmd.action.empty() && cmd.action.back() == ' ') cmd.action.pop_back();
            if (!cmd.action.empty()) cmds.push_back(cmd);
        }
    }
    return cmds;
}

TEST(Parse_BasicCommand) {
    auto cmds = ParseCommands("CMD:click:100,200");
    ASSERT_EQ((int)cmds.size(), 1);
    ASSERT_EQ(cmds[0].action, "click");
    ASSERT_EQ(cmds[0].params, "100,200");
    return true;
}

TEST(Parse_MultipleCommands) {
    auto cmds = ParseCommands("CMD:click:100,620;type:hello world;key:enter");
    ASSERT_EQ((int)cmds.size(), 3);
    ASSERT_EQ(cmds[0].action, "click");
    ASSERT_EQ(cmds[0].params, "100,620");
    ASSERT_EQ(cmds[1].action, "type");
    ASSERT_EQ(cmds[1].params, "hello world");
    ASSERT_EQ(cmds[2].action, "key");
    ASSERT_EQ(cmds[2].params, "enter");
    return true;
}

TEST(Parse_ChineseText) {
    // UTF-8 encoded Chinese: 继续根据下一个todo完成任务
    std::string input = "CMD:click:100,620;type:\xe7\xbb\xa7\xe7\xbb\xad\xe6\xa0\xb9\xe6\x8d\xae\xe4\xb8\x8b\xe4\xb8\x80\xe4\xb8\xaatodo\xe5\xae\x8c\xe6\x88\x90\xe4\xbb\xbb\xe5\x8a\xa1;key:enter";
    auto cmds = ParseCommands(input);
    ASSERT_EQ((int)cmds.size(), 3);
    ASSERT_EQ(cmds[0].action, "click");
    ASSERT_EQ(cmds[0].params, "100,620");
    ASSERT_EQ(cmds[1].action, "type");
    // Verify the full Chinese string is preserved
    std::string expected = "\xe7\xbb\xa7\xe7\xbb\xad\xe6\xa0\xb9\xe6\x8d\xae\xe4\xb8\x8b\xe4\xb8\x80\xe4\xb8\xaatodo\xe5\xae\x8c\xe6\x88\x90\xe4\xbb\xbb\xe5\x8a\xa1";
    ASSERT_EQ(cmds[1].params, expected);
    ASSERT_EQ(cmds[2].action, "key");
    ASSERT_EQ(cmds[2].params, "enter");
    return true;
}

TEST(Parse_ChineseWithColon) {
    // Chinese text containing colon-like characters shouldn't break parsing
    // type:你好:世界 — first colon splits action, rest is params
    std::string input = "CMD:type:\xe4\xbd\xa0\xe5\xa5\xbd:\xe4\xb8\x96\xe7\x95\x8c";
    auto cmds = ParseCommands(input);
    ASSERT_EQ((int)cmds.size(), 1);
    ASSERT_EQ(cmds[0].action, "type");
    // params should be everything after first colon: 你好:世界
    ASSERT_EQ(cmds[0].params, "\xe4\xbd\xa0\xe5\xa5\xbd:\xe4\xb8\x96\xe7\x95\x8c");
    return true;
}

TEST(Parse_NoSemicolonConfusion) {
    // Verify no UTF-8 byte equals 0x3B (semicolon)
    // Chinese chars use bytes 0x80-0xBF (continuation) and 0xC0+ (lead)
    std::string chinese = "\xe7\xbb\xa7\xe7\xbb\xad";  // 继续
    for (unsigned char c : chinese) {
        ASSERT_TRUE(c != ';');
    }
    return true;
}

TEST(Parse_ClipboardRoundtrip) {
    // Simulate: set clipboard as UTF-16, read back as UTF-8
    std::string original = "\xe7\xbb\xa7\xe7\xbb\xad\xe6\xa0\xb9\xe6\x8d\xae\xe4\xb8\x8b\xe4\xb8\x80\xe4\xb8\xaatodo\xe5\xae\x8c\xe6\x88\x90\xe4\xbb\xbb\xe5\x8a\xa1";

    // UTF-8 -> UTF-16
    int wlen = MultiByteToWideChar(CP_UTF8, 0, original.c_str(), -1, nullptr, 0);
    ASSERT_TRUE(wlen > 0);
    std::vector<wchar_t> wide(wlen);
    MultiByteToWideChar(CP_UTF8, 0, original.c_str(), -1, wide.data(), wlen);

    // UTF-16 -> UTF-8
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, nullptr, 0, nullptr, nullptr);
    ASSERT_TRUE(ulen > 0);
    std::string roundtrip(ulen - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, &roundtrip[0], ulen, nullptr, nullptr);

    ASSERT_EQ(roundtrip, original);
    return true;
}

int main() {
    return RunAllTests();
}
