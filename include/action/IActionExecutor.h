#pragma once
#include <string>
#include <windows.h>

class IActionExecutor {
public:
    virtual ~IActionExecutor() = default;
    virtual void Click(int x, int y) = 0;
    virtual void TypeText(const std::string& text) = 0;
    virtual void KeyPress(WORD vkCode) = 0;
    virtual void MoveMouse(int x, int y) = 0;
};
