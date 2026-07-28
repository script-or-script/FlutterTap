// FlutterTap native module -- by Eduardo Lopes
#include "json_min.h"

#include <cctype>
#include <cstdlib>

namespace {

class Parser {
public:
    explicit Parser(const std::string &text) : s(text), pos(0), len(text.size()) {}

    bool parseValue(JsonValue &out) {
        // Bounded recursion: parseValue -> parseObject/parseArray -> parseValue
        // is otherwise unlimited, and this parser runs inside *every* app
        // process. A config.json with a few hundred thousand '[' would overflow
        // the stack in each of them -- i.e. one bad file bricks every app on
        // the device until it is fixed over adb. Real configs nest 2 deep.
        struct DepthGuard {
            int &d;
            explicit DepthGuard(int &counter) : d(counter) { ++d; }
            ~DepthGuard() { --d; }
        } guard(depth);
        if (depth > kMaxDepth) return false;

        skipWs();
        if (pos >= len) return false;
        char c = s[pos];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') return parseString(out);
        if (c == 't' || c == 'f') return parseBool(out);
        if (c == 'n') return parseNull(out);
        if (c == '-' || isdigit(static_cast<unsigned char>(c))) return parseNumber(out);
        return false;
    }

private:
    static constexpr int kMaxDepth = 32;

    const std::string &s;
    size_t pos;
    size_t len;
    int depth = 0;

    void skipWs() {
        while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) pos++;
    }

    bool expect(char c) {
        skipWs();
        if (pos >= len || s[pos] != c) return false;
        pos++;
        return true;
    }

    bool parseObject(JsonValue &out) {
        out.type = JsonValue::Type::Object;
        if (!expect('{')) return false;
        skipWs();
        if (pos < len && s[pos] == '}') { pos++; return true; }
        while (true) {
            skipWs();
            JsonValue keyVal;
            if (!parseString(keyVal)) return false;
            if (!expect(':')) return false;
            JsonValue val;
            if (!parseValue(val)) return false;
            out.objValue[keyVal.strValue] = std::move(val);
            skipWs();
            if (pos < len && s[pos] == ',') { pos++; continue; }
            break;
        }
        return expect('}');
    }

    bool parseArray(JsonValue &out) {
        out.type = JsonValue::Type::Array;
        if (!expect('[')) return false;
        skipWs();
        if (pos < len && s[pos] == ']') { pos++; return true; }
        while (true) {
            JsonValue val;
            if (!parseValue(val)) return false;
            out.arrValue.push_back(std::move(val));
            skipWs();
            if (pos < len && s[pos] == ',') { pos++; continue; }
            break;
        }
        return expect(']');
    }

    bool parseString(JsonValue &out) {
        if (!expect('"')) return false;
        std::string result;
        while (pos < len && s[pos] != '"') {
            char c = s[pos++];
            if (c == '\\' && pos < len) {
                char esc = s[pos++];
                switch (esc) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    default: result += esc; break;
                }
            } else {
                result += c;
            }
        }
        if (pos >= len) return false;
        pos++; // closing quote
        out.type = JsonValue::Type::String;
        out.strValue = result;
        return true;
    }

    bool parseBool(JsonValue &out) {
        if (s.compare(pos, 4, "true") == 0) {
            pos += 4;
            out.type = JsonValue::Type::Bool;
            out.boolValue = true;
            return true;
        }
        if (s.compare(pos, 5, "false") == 0) {
            pos += 5;
            out.type = JsonValue::Type::Bool;
            out.boolValue = false;
            return true;
        }
        return false;
    }

    bool parseNull(JsonValue &out) {
        if (s.compare(pos, 4, "null") == 0) {
            pos += 4;
            out.type = JsonValue::Type::Null;
            return true;
        }
        return false;
    }

    bool parseNumber(JsonValue &out) {
        size_t start = pos;
        if (pos < len && s[pos] == '-') pos++;
        while (pos < len && (isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '.' || s[pos] == 'e' ||
                              s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) {
            pos++;
        }
        if (pos == start) return false;
        out.type = JsonValue::Type::Number;
        out.numValue = strtod(s.substr(start, pos - start).c_str(), nullptr);
        return true;
    }
};

} // namespace

bool json_parse(const std::string &text, JsonValue &out) {
    Parser parser(text);
    return parser.parseValue(out);
}

std::string JsonValue::getString(const std::string &key, const std::string &def) const {
    if (type != Type::Object) return def;
    auto it = objValue.find(key);
    if (it == objValue.end() || it->second.type != Type::String) return def;
    return it->second.strValue;
}

double JsonValue::getNumber(const std::string &key, double def) const {
    if (type != Type::Object) return def;
    auto it = objValue.find(key);
    if (it == objValue.end() || it->second.type != Type::Number) return def;
    return it->second.numValue;
}

bool JsonValue::getBool(const std::string &key, bool def) const {
    if (type != Type::Object) return def;
    auto it = objValue.find(key);
    if (it == objValue.end() || it->second.type != Type::Bool) return def;
    return it->second.boolValue;
}

const JsonValue *JsonValue::getArray(const std::string &key) const {
    if (type != Type::Object) return nullptr;
    auto it = objValue.find(key);
    if (it == objValue.end() || it->second.type != Type::Array) return nullptr;
    return &it->second;
}
