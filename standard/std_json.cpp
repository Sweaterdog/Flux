#include "std_json.h"
#include "../src/interpreter.h"
#include <sstream>
#include <algorithm>
#include <cctype>

// ============================================================================
// std.json — JSON parsing and stringification
//
// Provides:
//   JSON.parse(str)      -> Flux value (object, list, string, number, bool, nil)
//   JSON.stringify(val)   -> JSON string
//   JSON.stringify(val, indent) -> Pretty-printed JSON string
// ============================================================================

// --------------------------------------------------------------------------
// Internal JSON parser: turns a JSON string into Flux Values
// --------------------------------------------------------------------------
class JSONParser {
public:
    JSONParser(const std::string& src) : source(src), pos(0) {}

    Value parse() {
        skipWhitespace();
        Value result = parseValue();
        return result;
    }

private:
    std::string source;
    size_t pos;

    char peek() {
        if (pos >= source.size()) return '\0';
        return source[pos];
    }

    char advance() {
        return source[pos++];
    }

    void skipWhitespace() {
        while (pos < source.size() && std::isspace((unsigned char)source[pos])) pos++;
    }

    void expect(char c) {
        skipWhitespace();
        if (pos >= source.size() || source[pos] != c) {
            throw FluxException("error", std::string("JSON parse error: expected '") + c + "'");
        }
        pos++;
    }

    Value parseValue() {
        skipWhitespace();
        if (pos >= source.size()) return Value::nil();

        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || std::isdigit((unsigned char)c)) return parseNumber();

        throw FluxException("error", std::string("JSON parse error: unexpected character '") + c + "'");
    }

    Value parseString() {
        expect('"');
        std::string result;
        while (pos < source.size() && source[pos] != '"') {
            if (source[pos] == '\\') {
                pos++;
                if (pos >= source.size()) break;
                switch (source[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Parse 4-digit hex unicode escape
                        std::string hex;
                        for (int i = 0; i < 4 && pos + 1 < source.size(); i++) {
                            pos++;
                            hex += source[pos];
                        }
                        int codepoint = std::stoi(hex, nullptr, 16);
                        if (codepoint < 0x80) {
                            result += (char)codepoint;
                        } else if (codepoint < 0x800) {
                            result += (char)(0xC0 | (codepoint >> 6));
                            result += (char)(0x80 | (codepoint & 0x3F));
                        } else {
                            result += (char)(0xE0 | (codepoint >> 12));
                            result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                            result += (char)(0x80 | (codepoint & 0x3F));
                        }
                        break;
                    }
                    default: result += source[pos]; break;
                }
            } else {
                result += source[pos];
            }
            pos++;
        }
        if (pos < source.size()) pos++; // skip closing "
        return Value::fromString(result);
    }

    Value parseNumber() {
        size_t start = pos;
        if (source[pos] == '-') pos++;
        while (pos < source.size() && std::isdigit((unsigned char)source[pos])) pos++;

        bool isFloat = false;
        if (pos < source.size() && source[pos] == '.') {
            isFloat = true;
            pos++;
            while (pos < source.size() && std::isdigit((unsigned char)source[pos])) pos++;
        }
        if (pos < source.size() && (source[pos] == 'e' || source[pos] == 'E')) {
            isFloat = true;
            pos++;
            if (pos < source.size() && (source[pos] == '+' || source[pos] == '-')) pos++;
            while (pos < source.size() && std::isdigit((unsigned char)source[pos])) pos++;
        }

        std::string numStr = source.substr(start, pos - start);
        if (isFloat) {
            return Value::fromFloat(std::stod(numStr));
        } else {
            long long val = std::stoll(numStr);
            if (val >= INT32_MIN && val <= INT32_MAX) {
                return Value::fromInt((int32_t)val);
            }
            return Value::fromLong((int64_t)val);
        }
    }

    Value parseBool() {
        if (source.substr(pos, 4) == "true") {
            pos += 4;
            return Value::fromBool(true);
        }
        if (source.substr(pos, 5) == "false") {
            pos += 5;
            return Value::fromBool(false);
        }
        throw FluxException("error", "JSON parse error: invalid boolean");
    }

    Value parseNull() {
        if (source.substr(pos, 4) == "null") {
            pos += 4;
            return Value::nil();
        }
        throw FluxException("error", "JSON parse error: invalid null");
    }

    Value parseArray() {
        expect('[');
        auto list = std::make_shared<std::vector<Value>>();
        skipWhitespace();
        if (peek() == ']') { pos++; return Value::fromList(list); }

        list->push_back(parseValue());
        while (peek() != ']') {
            expect(',');
            list->push_back(parseValue());
        }
        pos++; // skip ]
        return Value::fromList(list);
    }

    Value parseObject() {
        expect('{');
        auto cls = std::make_shared<FluxClass>();
        cls->name = "JSONObject";
        auto obj = std::make_shared<FluxObject>();
        obj->classDef = cls;

        skipWhitespace();
        if (peek() == '}') { pos++; goto done; }

        {
            // Parse first key-value
            Value keyVal = parseString();
            expect(':');
            Value val = parseValue();
            obj->fields[keyVal.toString()] = val;
        }

        while (peek() != '}') {
            expect(',');
            skipWhitespace();
            Value keyVal = parseString();
            expect(':');
            Value val = parseValue();
            obj->fields[keyVal.toString()] = val;
        }
        pos++; // skip }

    done:
        Value result;
        result.type = ValueType::OBJECT;
        result.objectVal = obj;
        return result;
    }
};

// --------------------------------------------------------------------------
// Internal JSON stringifier: turns Flux Values into JSON strings
// --------------------------------------------------------------------------
static std::string escapeJSONString(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    result += '"';
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    result += '"';
    return result;
}

static std::string stringifyValue(const Value& val, int indent, int currentIndent);

static std::string stringifyValue(const Value& val, int indent, int currentIndent) {
    switch (val.type) {
        case ValueType::NIL:
            return "null";
        case ValueType::BOOL:
            return val.boolVal ? "true" : "false";
        case ValueType::INT:
            return std::to_string(val.intVal);
        case ValueType::LONG:
            return std::to_string(val.longVal);
        case ValueType::FLOAT: {
            std::ostringstream oss;
            oss << val.floatVal;
            std::string s = oss.str();
            // Ensure it has a decimal point for JSON
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
                s += ".0";
            }
            return s;
        }
        case ValueType::CHAR:
            return escapeJSONString(std::string(1, val.charVal));
        case ValueType::BYTE:
            return std::to_string(val.byteVal);
        case ValueType::STRING:
            return escapeJSONString(val.stringVal);
        case ValueType::LIST: {
            if (!val.listVal || val.listVal->empty()) return "[]";
            std::string result = "[";
            int nextIndent = currentIndent + indent;
            std::string sep = indent > 0 ? ",\n" : ",";
            std::string pad = indent > 0 ? std::string(nextIndent, ' ') : "";
            std::string closePad = indent > 0 ? std::string(currentIndent, ' ') : "";
            if (indent > 0) result += "\n";
            for (size_t i = 0; i < val.listVal->size(); i++) {
                if (i > 0) result += sep;
                result += pad + stringifyValue((*val.listVal)[i], indent, nextIndent);
            }
            if (indent > 0) result += "\n" + closePad;
            result += "]";
            return result;
        }
        case ValueType::OBJECT: {
            if (!val.objectVal) return "null";
            auto& fields = val.objectVal->fields;
            if (fields.empty()) return "{}";
            std::string result = "{";
            int nextIndent = currentIndent + indent;
            std::string sep = indent > 0 ? ",\n" : ",";
            std::string pad = indent > 0 ? std::string(nextIndent, ' ') : "";
            std::string closePad = indent > 0 ? std::string(currentIndent, ' ') : "";
            if (indent > 0) result += "\n";
            bool first = true;
            for (auto& [key, value] : fields) {
                // Skip native function fields (not serializable)
                if (value.type == ValueType::NATIVE_FUNCTION || value.type == ValueType::FUNCTION) continue;
                if (!first) result += sep;
                first = false;
                result += pad + escapeJSONString(key) + (indent > 0 ? ": " : ":") +
                          stringifyValue(value, indent, nextIndent);
            }
            if (indent > 0) result += "\n" + closePad;
            result += "}";
            return result;
        }
        default:
            return "null";
    }
}

// ============================================================================
// Registration
// ============================================================================

void registerStdJSON(std::shared_ptr<Environment> env, Interpreter& interp) {
    auto jsonClass = std::make_shared<FluxClass>();
    jsonClass->name = "JSON";
    auto jsonObj = std::make_shared<FluxObject>();
    jsonObj->classDef = jsonClass;

    // JSON.parse(str) -> Value
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty() || args[0].type != ValueType::STRING) {
                throw FluxException("error", "JSON.parse requires a string argument");
            }
            JSONParser parser(args[0].stringVal);
            return parser.parse();
        };
        jsonObj->fields["parse"] = fn;
    }

    // JSON.stringify(val) / JSON.stringify(val, indent)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromString("null");
            int indent = 0;
            if (args.size() > 1) {
                indent = (int)args[1].toNumber();
            }
            return Value::fromString(stringifyValue(args[0], indent, 0));
        };
        jsonObj->fields["stringify"] = fn;
    }

    Value jsonVal;
    jsonVal.type = ValueType::OBJECT;
    jsonVal.objectVal = jsonObj;
    env->define("JSON", jsonVal, "object");
}
