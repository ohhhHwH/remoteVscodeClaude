#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json {

struct Value;
using Object = std::unordered_map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool b) : data(b) {}
    Value(double d) : data(d) {}
    Value(const std::string& s) : data(s) {}
    Value(std::string&& s) : data(std::move(s)) {}
    Value(const Array& a) : data(a) {}
    Value(Array&& a) : data(std::move(a)) {}
    Value(const Object& o) : data(o) {}
    Value(Object&& o) : data(std::move(o)) {}

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_number() const { return std::holds_alternative<double>(data); }
    bool is_array() const { return std::holds_alternative<Array>(data); }
    bool is_object() const { return std::holds_alternative<Object>(data); }

    const std::string& as_string() const { return std::get<std::string>(data); }
    double as_number() const { return std::get<double>(data); }
    int as_int() const { return (int)std::get<double>(data); }
    const Array& as_array() const { return std::get<Array>(data); }
    const Object& as_object() const { return std::get<Object>(data); }

    const Value& operator[](const std::string& key) const {
        return std::get<Object>(data).at(key);
    }
    const Value& operator[](size_t idx) const {
        return std::get<Array>(data).at(idx);
    }
};

class Parser {
public:
    Value Parse(const std::string& input) {
        src_ = input;
        pos_ = 0;
        return ParseValue();
    }

    Value ParseFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
        std::stringstream ss;
        ss << f.rdbuf();
        return Parse(ss.str());
    }

private:
    std::string src_;
    size_t pos_ = 0;

    void SkipWhitespace() {
        while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\n' || src_[pos_] == '\r' || src_[pos_] == '\t'))
            pos_++;
    }

    char Peek() { SkipWhitespace(); return pos_ < src_.size() ? src_[pos_] : '\0'; }
    char Next() { SkipWhitespace(); return src_[pos_++]; }

    Value ParseValue() {
        char c = Peek();
        if (c == '"') return ParseString();
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        return ParseNumber();
    }

    Value ParseString() {
        Next();
        std::string result;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            if (src_[pos_] == '\\') {
                pos_++;
                switch (src_[pos_]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    default: result += src_[pos_]; break;
                }
            } else {
                result += src_[pos_];
            }
            pos_++;
        }
        pos_++;
        return Value(result);
    }

    Value ParseNumber() {
        SkipWhitespace();
        size_t start = pos_;
        if (src_[pos_] == '-') pos_++;
        while (pos_ < src_.size() && (isdigit(src_[pos_]) || src_[pos_] == '.')) pos_++;
        return Value(std::stod(src_.substr(start, pos_ - start)));
    }

    Value ParseBool() {
        SkipWhitespace();
        if (src_.substr(pos_, 4) == "true") { pos_ += 4; return Value(true); }
        pos_ += 5; return Value(false);
    }

    Value ParseNull() {
        pos_ += 4; return Value(nullptr);
    }

    Value ParseArray() {
        Next();
        Array arr;
        if (Peek() == ']') { pos_++; return Value(std::move(arr)); }
        while (true) {
            arr.push_back(ParseValue());
            if (Peek() == ']') { pos_++; break; }
            Next();
        }
        return Value(std::move(arr));
    }

    Value ParseObject() {
        Next();
        Object obj;
        if (Peek() == '}') { pos_++; return Value(std::move(obj)); }
        while (true) {
            auto key = ParseString();
            Next();
            obj[key.as_string()] = ParseValue();
            if (Peek() == '}') { pos_++; break; }
            Next();
        }
        return Value(std::move(obj));
    }
};

inline std::string Serialize(const Value& v) {
    if (v.is_null()) return "null";
    if (std::holds_alternative<bool>(v.data)) return std::get<bool>(v.data) ? "true" : "false";
    if (v.is_number()) {
        double d = v.as_number();
        if (d == (int)d) return std::to_string((int)d);
        return std::to_string(d);
    }
    if (v.is_string()) return "\"" + v.as_string() + "\"";
    if (v.is_array()) {
        std::string s = "[";
        for (size_t i = 0; i < v.as_array().size(); i++) {
            if (i) s += ",";
            s += Serialize(v.as_array()[i]);
        }
        return s + "]";
    }
    if (v.is_object()) {
        std::string s = "{";
        bool first = true;
        for (auto& [k, val] : v.as_object()) {
            if (!first) s += ",";
            s += "\"" + k + "\":" + Serialize(val);
            first = false;
        }
        return s + "}";
    }
    return "";
}

} // namespace json
