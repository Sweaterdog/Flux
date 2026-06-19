#include "std_regex.h"
#include "../src/interpreter.h"
#include <regex>

// ============================================================================
// std.regex — Regular expression support
//
// Provides:
//   Regex(pattern)              -> regex object
//     .match(str)               -> bool (full match)
//     .search(str)              -> string|nil (first match)
//     .findAll(str)             -> list of strings (all matches)
//     .replace(str, replacement)-> string
//     .split(str)               -> list of strings
//     .groups(str)              -> list of captured groups
//     .pattern                  -> string (the pattern)
// ============================================================================

void registerStdRegex(std::shared_ptr<Environment> env, Interpreter& interp) {

    // Regex(pattern) constructor
    Value regexCtor;
    regexCtor.type = ValueType::NATIVE_FUNCTION;
    regexCtor.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            throw FluxException("error", "Regex() requires a string pattern argument");
        }
        std::string pattern = args[0].stringVal;

        // Compile the regex
        std::shared_ptr<std::regex> re;
        try {
            re = std::make_shared<std::regex>(pattern, std::regex::ECMAScript);
        } catch (const std::regex_error& e) {
            throw FluxException("error", std::string("Invalid regex pattern: ") + e.what());
        }

        auto cls = std::make_shared<FluxClass>();
        cls->name = "Regex";
        auto obj = std::make_shared<FluxObject>();
        obj->classDef = cls;

        // .pattern
        obj->fields["pattern"] = Value::fromString(pattern);

        // .match(str) -> bool (tests if the ENTIRE string matches)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [re](Interpreter&, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromBool(false);
                return Value::fromBool(std::regex_match(args[0].toString(), *re));
            };
            obj->fields["match"] = fn;
        }

        // .search(str) -> string|nil (first match found anywhere)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [re](Interpreter&, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::nil();
                std::string s = args[0].toString();
                std::smatch m;
                if (std::regex_search(s, m, *re)) {
                    return Value::fromString(m[0].str());
                }
                return Value::nil();
            };
            obj->fields["search"] = fn;
        }

        // .findAll(str) -> list of all matched substrings
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [re](Interpreter&, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::makeList();
                std::string s = args[0].toString();
                auto list = std::make_shared<std::vector<Value>>();
                auto begin = std::sregex_iterator(s.begin(), s.end(), *re);
                auto end = std::sregex_iterator();
                for (auto it = begin; it != end; ++it) {
                    list->push_back(Value::fromString((*it)[0].str()));
                }
                return Value::fromList(list);
            };
            obj->fields["findAll"] = fn;
        }

        // .replace(str, replacement) -> string
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [re](Interpreter&, std::vector<Value> args) -> Value {
                if (args.size() < 2) throw FluxException("error", "Regex.replace requires (str, replacement)");
                std::string s = args[0].toString();
                std::string replacement = args[1].toString();
                return Value::fromString(std::regex_replace(s, *re, replacement));
            };
            obj->fields["replace"] = fn;
        }

        // .split(str) -> list of strings
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [re](Interpreter&, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::makeList();
                std::string s = args[0].toString();
                auto list = std::make_shared<std::vector<Value>>();
                std::sregex_token_iterator it(s.begin(), s.end(), *re, -1);
                std::sregex_token_iterator end;
                for (; it != end; ++it) {
                    list->push_back(Value::fromString(it->str()));
                }
                return Value::fromList(list);
            };
            obj->fields["split"] = fn;
        }

        // .groups(str) -> list of captured groups from first match
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [re](Interpreter&, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::makeList();
                std::string s = args[0].toString();
                std::smatch m;
                auto list = std::make_shared<std::vector<Value>>();
                if (std::regex_search(s, m, *re)) {
                    // Skip group 0 (full match), return captured groups
                    for (size_t i = 1; i < m.size(); i++) {
                        if (m[i].matched) {
                            list->push_back(Value::fromString(m[i].str()));
                        } else {
                            list->push_back(Value::nil());
                        }
                    }
                }
                return Value::fromList(list);
            };
            obj->fields["groups"] = fn;
        }

        Value result;
        result.type = ValueType::OBJECT;
        result.objectVal = obj;
        return result;
    };
    env->define("Regex", regexCtor, "native_function");
}
