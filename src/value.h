#pragma once

#include "ast.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <cmath>
#include <random>
#include <stdexcept>

// Forward declarations
class Environment;
class Interpreter;

// ============================================================================
// Runtime value types for Flux
// ============================================================================

// Forward declarations for complex types
struct FluxFunction;
struct FluxClass;
struct FluxObject;
struct FluxEnum;
struct FluxStruct;

// Value type enumeration
enum class ValueType {
    NIL,
    BOOL,
    CHAR,
    BYTE,
    INT,
    LONG,
    FLOAT,
    STRING,
    LIST,
    MAP,
    OBJECT,
    FUNCTION,
    NATIVE_FUNCTION,
    CLASS_DEF,
    STRUCT_DEF,
    ENUM_DEF,
    ENUM_VALUE,
};

// ============================================================================
// Value class - represents any Flux runtime value
// ============================================================================

class Value {
public:
    ValueType type;

    // Primitive storage
    bool boolVal = false;
    char charVal = '\0';
    uint8_t byteVal = 0;
    int32_t intVal = 0;
    int64_t longVal = 0;
    double floatVal = 0.0;
    std::string stringVal;

    // Complex type storage (shared ownership)
    std::shared_ptr<std::vector<Value>> listVal;
    std::shared_ptr<std::unordered_map<std::string, Value>> mapVal;
    std::shared_ptr<FluxObject> objectVal;
    std::shared_ptr<FluxFunction> functionVal;
    std::shared_ptr<FluxClass> classVal;
    std::shared_ptr<FluxStruct> structVal;
    std::shared_ptr<FluxEnum> enumVal;

    // For enum values: which enum + which member
    std::string enumName;
    std::string enumMember;
    int enumIntVal = 0;

    // Native function pointer type
    using NativeFn = std::function<Value(Interpreter&, std::vector<Value>)>;
    NativeFn nativeFn;

    // ========================================================================
    // Constructors
    // ========================================================================

    Value() : type(ValueType::NIL) {}

    static Value nil() {
        Value v;
        v.type = ValueType::NIL;
        return v;
    }

    static Value fromBool(bool b) {
        Value v;
        v.type = ValueType::BOOL;
        v.boolVal = b;
        return v;
    }

    static Value fromChar(char c) {
        Value v;
        v.type = ValueType::CHAR;
        v.charVal = c;
        return v;
    }

    static Value fromByte(uint8_t b) {
        Value v;
        v.type = ValueType::BYTE;
        v.byteVal = b;
        return v;
    }

    static Value fromInt(int32_t i) {
        Value v;
        v.type = ValueType::INT;
        v.intVal = i;
        return v;
    }

    static Value fromLong(int64_t l) {
        Value v;
        v.type = ValueType::LONG;
        v.longVal = l;
        return v;
    }

    static Value fromFloat(double f) {
        Value v;
        v.type = ValueType::FLOAT;
        v.floatVal = f;
        return v;
    }

    static Value fromString(const std::string& s) {
        Value v;
        v.type = ValueType::STRING;
        v.stringVal = s;
        return v;
    }

    static Value fromList(std::shared_ptr<std::vector<Value>> list) {
        Value v;
        v.type = ValueType::LIST;
        v.listVal = list;
        return v;
    }

    static Value makeList() {
        Value v;
        v.type = ValueType::LIST;
        v.listVal = std::make_shared<std::vector<Value>>();
        return v;
    }

    // ========================================================================
    // Conversion & Utility
    // ========================================================================

    // Convert value to string representation (for print, interpolation, etc.)
    std::string toString() const {
        switch (type) {
            case ValueType::NIL: return "null";
            case ValueType::BOOL: return boolVal ? "true" : "false";
            case ValueType::CHAR: return std::string(1, charVal);
            case ValueType::BYTE: return std::to_string((int)byteVal);
            case ValueType::INT: return std::to_string(intVal);
            case ValueType::LONG: return std::to_string(longVal);
            case ValueType::FLOAT: {
                std::ostringstream oss;
                oss << floatVal;
                return oss.str();
            }
            case ValueType::STRING: return stringVal;
            case ValueType::LIST: {
                std::string result = "[";
                if (listVal) {
                    for (size_t i = 0; i < listVal->size(); i++) {
                        if (i > 0) result += ", ";
                        if ((*listVal)[i].type == ValueType::STRING) {
                            result += "\"" + (*listVal)[i].toString() + "\"";
                        } else {
                            result += (*listVal)[i].toString();
                        }
                    }
                }
                result += "]";
                return result;
            }
            case ValueType::OBJECT: return "<object>";
            case ValueType::FUNCTION: return "<function>";
            case ValueType::NATIVE_FUNCTION: return "<native function>";
            case ValueType::CLASS_DEF: return "<class>";
            case ValueType::STRUCT_DEF: return "<struct>";
            case ValueType::ENUM_DEF: return "<enum>";
            case ValueType::ENUM_VALUE: return enumName + "." + enumMember;
            case ValueType::MAP: return "<map>";
        }
        return "<unknown>";
    }

    // Get numeric value as double (for arithmetic)
    double toNumber() const {
        switch (type) {
            case ValueType::INT: return (double)intVal;
            case ValueType::LONG: return (double)longVal;
            case ValueType::FLOAT: return floatVal;
            case ValueType::BYTE: return (double)byteVal;
            case ValueType::BOOL: return boolVal ? 1.0 : 0.0;
            case ValueType::CHAR: return (double)charVal;
            default: return 0.0;
        }
    }

    // Check if the value is "truthy"
    bool isTruthy() const {
        switch (type) {
            case ValueType::NIL: return false;
            case ValueType::BOOL: return boolVal;
            case ValueType::INT: return intVal != 0;
            case ValueType::LONG: return longVal != 0;
            case ValueType::FLOAT: return floatVal != 0.0;
            case ValueType::BYTE: return byteVal != 0;
            case ValueType::CHAR: return charVal != '\0';
            case ValueType::STRING: return !stringVal.empty();
            case ValueType::LIST: return listVal && !listVal->empty();
            default: return true;
        }
    }

    // Check if type is numeric
    bool isNumeric() const {
        return type == ValueType::INT || type == ValueType::LONG ||
               type == ValueType::FLOAT || type == ValueType::BYTE;
    }

    // Identity equality (==): cross-type comparison when values are representable
    bool identityEquals(const Value& other) const {
        // Same type comparison
        if (type == other.type) {
            switch (type) {
                case ValueType::NIL: return true;
                case ValueType::BOOL: return boolVal == other.boolVal;
                case ValueType::CHAR: return charVal == other.charVal;
                case ValueType::BYTE: return byteVal == other.byteVal;
                case ValueType::INT: return intVal == other.intVal;
                case ValueType::LONG: return longVal == other.longVal;
                case ValueType::FLOAT: return floatVal == other.floatVal;
                case ValueType::STRING: return stringVal == other.stringVal;
                case ValueType::ENUM_VALUE:
                    return enumName == other.enumName && enumMember == other.enumMember;
                default: return false;
            }
        }

        // Cross-type numeric comparison
        if (isNumeric() && other.isNumeric()) {
            return toNumber() == other.toNumber();
        }

        // String to number comparison: "10" == 10 is true
        if (type == ValueType::STRING && other.isNumeric()) {
            try {
                double val = std::stod(stringVal);
                return val == other.toNumber();
            } catch (...) {
                return false;
            }
        }
        if (isNumeric() && other.type == ValueType::STRING) {
            try {
                double val = std::stod(other.stringVal);
                return toNumber() == val;
            } catch (...) {
                return false;
            }
        }

        // Bool to int: 1 == true, 0 == false
        if (type == ValueType::BOOL && other.isNumeric()) {
            return (boolVal ? 1.0 : 0.0) == other.toNumber();
        }
        if (isNumeric() && other.type == ValueType::BOOL) {
            return toNumber() == (other.boolVal ? 1.0 : 0.0);
        }

        return false;
    }

    // Numeric strict equality (=num=): both must be numeric types
    bool numericEquals(const Value& other) const {
        if (!isNumeric() || !other.isNumeric()) return false;
        return toNumber() == other.toNumber();
    }

    // Word strict equality (=word=): both must be strings
    bool wordEquals(const Value& other) const {
        if (type != ValueType::STRING || other.type != ValueType::STRING) return false;
        return stringVal == other.stringVal;
    }

    // Get the Flux type name for this value
    std::string typeName() const {
        switch (type) {
            case ValueType::NIL:    return "void";
            case ValueType::BOOL:   return "bool";
            case ValueType::CHAR:   return "char";
            case ValueType::BYTE:   return "byte";
            case ValueType::INT:    return "int";
            case ValueType::LONG:   return "long";
            case ValueType::FLOAT:  return "float";
            case ValueType::STRING: return "string";
            case ValueType::LIST:   return "List";
            case ValueType::MAP:    return "Map";
            case ValueType::OBJECT: return "object";
            case ValueType::FUNCTION:        return "function";
            case ValueType::NATIVE_FUNCTION: return "native_function";
            case ValueType::CLASS_DEF:  return "class";
            case ValueType::STRUCT_DEF: return "struct";
            case ValueType::ENUM_DEF:   return "enum";
            case ValueType::ENUM_VALUE: return enumName;
        }
        return "unknown";
    }
};

// ============================================================================
// FluxFunction - A user-defined function
// ============================================================================

struct FluxFunction {
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;    // "" = default (int), "void" = no return
    ASTNodePtr body;           // BlockNode
    std::shared_ptr<Environment> closure;
    bool isMethod = false;
    bool isInitializer = false;
    std::shared_ptr<FluxObject> boundObject;  // For method calls, ref to 'this'
};

// ============================================================================
// FluxClass - A class definition
// ============================================================================

struct FluxClass {
    std::string name;
    std::string parentName;
    std::shared_ptr<FluxClass> parent;
    std::unordered_map<std::string, Value> methods;     // name -> function value
    std::unordered_map<std::string, std::string> fieldTypes;  // name -> type
    std::unordered_map<std::string, Value> fieldDefaults;     // name -> default value
    std::unordered_map<std::string, std::string> fieldAccess; // name -> "public"/"private"/"protected"
    std::vector<std::string> interfaces;

    // Find a method (including inherited)
    Value findMethod(const std::string& name) const {
        auto it = methods.find(name);
        if (it != methods.end()) return it->second;
        if (parent) return parent->findMethod(name);
        return Value::nil();
    }
};

// ============================================================================
// FluxObject - An instance of a class
// ============================================================================

struct FluxObject {
    std::shared_ptr<FluxClass> classDef;
    std::unordered_map<std::string, Value> fields;

    Value getField(const std::string& name) const {
        auto it = fields.find(name);
        if (it != fields.end()) return it->second;
        return Value::nil();
    }

    void setField(const std::string& name, const Value& val) {
        fields[name] = val;
    }
};

// ============================================================================
// FluxStruct - A struct definition
// ============================================================================

struct FluxStruct {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields; // {name, type}
};

// ============================================================================
// FluxEnum - An enum definition
// ============================================================================

struct FluxEnum {
    std::string name;
    std::unordered_map<std::string, int> values; // member name -> int value
};

// ============================================================================
// Control flow signals (implemented as C++ exceptions)
// ============================================================================

struct BreakSignal {};
struct ContinueSignal {};

struct ReturnSignal {
    Value value;
    ReturnSignal(const Value& v) : value(v) {}
};

struct PanicSignal {
    std::string message;
    PanicSignal(const std::string& msg) : message(msg) {}
};

struct FluxException {
    std::string errorType;
    std::string message;
    int code = 0;
    std::string stack;

    FluxException(const std::string& type, const std::string& msg)
        : errorType(type), message(msg) {}
};
