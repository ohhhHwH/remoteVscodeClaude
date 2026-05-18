#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>

struct TestCase {
    std::string name;
    std::function<bool()> func;
};

static std::vector<TestCase>& GetTests() {
    static std::vector<TestCase> tests;
    return tests;
}

#define TEST(name) \
    bool test_##name(); \
    static bool _reg_##name = (GetTests().push_back({#name, test_##name}), true); \
    bool test_##name()

#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "  FAIL: " #expr << " at line " << __LINE__ << std::endl; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "  FAIL: " #a " != " #b " at line " << __LINE__ << std::endl; return false; } } while(0)

inline int RunAllTests() {
    int passed = 0, failed = 0;
    for (auto& t : GetTests()) {
        std::cout << "[RUN ] " << t.name << std::endl;
        if (t.func()) {
            std::cout << "[PASS] " << t.name << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] " << t.name << std::endl;
            failed++;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed." << std::endl;
    return failed > 0 ? 1 : 0;
}
