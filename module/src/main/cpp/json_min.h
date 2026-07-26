// FlutterTap native module -- by Eduardo Lopes
//
// Minimal read-only JSON parser. This is intentionally NOT a general purpose
// JSON library: it only implements what is needed to read config.json
// (objects, arrays, strings, numbers, booleans, null), so we avoid vendoring
// a large third-party JSON dependency just to read one small config file.
#pragma once

#include <map>
#include <string>
#include <vector>

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolValue = false;
    double numValue = 0.0;
    std::string strValue;
    std::vector<JsonValue> arrValue;
    std::map<std::string, JsonValue> objValue;

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }

    // Convenience accessors used by module_config.cpp. Return the provided
    // default when the key is missing or has an unexpected type.
    std::string getString(const std::string &key, const std::string &def) const;
    double getNumber(const std::string &key, double def) const;
    bool getBool(const std::string &key, bool def) const;
    const JsonValue *getArray(const std::string &key) const;
};

// Parses `text` into `out`. Returns false on malformed JSON.
bool json_parse(const std::string &text, JsonValue &out);
