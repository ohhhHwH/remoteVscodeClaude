#include "TestFramework.h"
#include <string>
#include <windows.h>

class MockAction {
public:
    int clickCount = 0;
    int typeCount = 0;
    int keyPressCount = 0;
    int moveCount = 0;
    std::string lastTyped;
    WORD lastKey = 0;
    int lastX = 0, lastY = 0;

    void Click(int x, int y) {
        clickCount++;
        lastX = x; lastY = y;
    }
    void TypeText(const std::string& text) {
        typeCount++;
        lastTyped = text;
    }
    void KeyPress(WORD vkCode) {
        keyPressCount++;
        lastKey = vkCode;
    }
    void MoveMouse(int x, int y) {
        moveCount++;
        lastX = x; lastY = y;
    }
};

TEST(Action_Click) {
    MockAction action;
    action.Click(100, 200);
    ASSERT_EQ(action.clickCount, 1);
    ASSERT_EQ(action.lastX, 100);
    ASSERT_EQ(action.lastY, 200);
    return true;
}

TEST(Action_TypeText) {
    MockAction action;
    action.TypeText("hello");
    ASSERT_EQ(action.typeCount, 1);
    ASSERT_EQ(action.lastTyped, "hello");
    return true;
}

TEST(Action_KeyPress) {
    MockAction action;
    action.KeyPress(VK_RETURN);
    ASSERT_EQ(action.keyPressCount, 1);
    ASSERT_EQ(action.lastKey, (WORD)VK_RETURN);
    return true;
}

TEST(Action_MoveMouse) {
    MockAction action;
    action.MoveMouse(500, 300);
    ASSERT_EQ(action.moveCount, 1);
    ASSERT_EQ(action.lastX, 500);
    ASSERT_EQ(action.lastY, 300);
    return true;
}

TEST(Action_MultipleOperations) {
    MockAction action;
    action.Click(10, 20);
    action.TypeText("test");
    action.KeyPress(VK_TAB);
    action.MoveMouse(0, 0);

    ASSERT_EQ(action.clickCount, 1);
    ASSERT_EQ(action.typeCount, 1);
    ASSERT_EQ(action.keyPressCount, 1);
    ASSERT_EQ(action.moveCount, 1);
    return true;
}

int main() {
    return RunAllTests();
}
