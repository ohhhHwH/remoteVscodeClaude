#include "TestFramework.h"
#include "core/Json.h"

TEST(Json_ParseNumber) {
    json::Parser p;
    auto v = p.Parse("42");
    ASSERT_TRUE(v.is_number());
    ASSERT_EQ(v.as_int(), 42);
    return true;
}

TEST(Json_ParseString) {
    json::Parser p;
    auto v = p.Parse("\"hello world\"");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string(), "hello world");
    return true;
}

TEST(Json_ParseEscapedString) {
    json::Parser p;
    auto v = p.Parse("\"line1\\nline2\"");
    ASSERT_EQ(v.as_string(), "line1\nline2");
    return true;
}

TEST(Json_ParseBool) {
    json::Parser p;
    auto t = p.Parse("true");
    auto f = p.Parse("false");
    ASSERT_EQ(std::get<bool>(t.data), true);
    ASSERT_EQ(std::get<bool>(f.data), false);
    return true;
}

TEST(Json_ParseNull) {
    json::Parser p;
    auto v = p.Parse("null");
    ASSERT_TRUE(v.is_null());
    return true;
}

TEST(Json_ParseArray) {
    json::Parser p;
    auto v = p.Parse("[1, 2, 3]");
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ(v.as_array().size(), (size_t)3);
    ASSERT_EQ(v[0].as_int(), 1);
    ASSERT_EQ(v[2].as_int(), 3);
    return true;
}

TEST(Json_ParseObject) {
    json::Parser p;
    auto v = p.Parse("{\"name\": \"test\", \"value\": 123}");
    ASSERT_TRUE(v.is_object());
    ASSERT_EQ(v["name"].as_string(), "test");
    ASSERT_EQ(v["value"].as_int(), 123);
    return true;
}

TEST(Json_ParseNested) {
    json::Parser p;
    auto v = p.Parse("{\"arr\": [1, {\"x\": 2}]}");
    ASSERT_EQ(v["arr"][0].as_int(), 1);
    ASSERT_EQ(v["arr"][1]["x"].as_int(), 2);
    return true;
}

TEST(Json_Serialize) {
    json::Object obj;
    obj["key"] = json::Value(std::string("val"));
    obj["num"] = json::Value(3.0);
    json::Value v(std::move(obj));
    std::string s = json::Serialize(v);
    ASSERT_TRUE(s.find("\"key\":\"val\"") != std::string::npos);
    ASSERT_TRUE(s.find("\"num\":3") != std::string::npos);
    return true;
}

int main() {
    return RunAllTests();
}
