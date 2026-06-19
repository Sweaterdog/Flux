#pragma once

#include "value.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>

// ============================================================================
// Environment - Scoped variable storage for the Flux interpreter
//
// Each environment has an optional parent, forming a chain for lexical scoping.
// Variable lookup walks up the chain until the variable is found.
// ============================================================================

class Environment : public std::enable_shared_from_this<Environment> {
public:
    std::shared_ptr<Environment> parent;

    Environment() : parent(nullptr) {}
    Environment(std::shared_ptr<Environment> parent) : parent(parent) {}

    // Define a new variable in the current scope
    void define(const std::string& name, const Value& value,
                const std::string& typeName = "", bool isConst = false) {
        values[name] = value;
        types[name] = typeName;
        constFlags[name] = isConst;
    }

    // Get a variable's value (searches up the scope chain)
    Value get(const std::string& name) const {
        auto it = values.find(name);
        if (it != values.end()) return it->second;
        if (parent) return parent->get(name);
        throw std::runtime_error("Undefined variable '" + name + "'");
    }

    // Check if a variable exists in any scope
    bool has(const std::string& name) const {
        if (values.count(name)) return true;
        if (parent) return parent->has(name);
        return false;
    }

    // Check if a variable exists in THIS scope only
    bool hasLocal(const std::string& name) const {
        return values.count(name) > 0;
    }

    // Set a variable's value (finds it in scope chain, then updates)
    void set(const std::string& name, const Value& value) {
        auto it = values.find(name);
        if (it != values.end()) {
            if (constFlags.count(name) && constFlags[name]) {
                throw std::runtime_error("Cannot reassign constant '" + name + "'");
            }
            it->second = value;
            return;
        }
        if (parent) {
            parent->set(name, value);
            return;
        }
        throw std::runtime_error("Undefined variable '" + name + "'");
    }

    // Get the type name of a variable
    std::string getType(const std::string& name) const {
        auto it = types.find(name);
        if (it != types.end()) return it->second;
        if (parent) return parent->getType(name);
        return "";
    }

    // Check if a variable is constant
    bool isConst(const std::string& name) const {
        auto it = constFlags.find(name);
        if (it != constFlags.end()) return it->second;
        if (parent) return parent->isConst(name);
        return false;
    }

    // Re-define a variable with a new type (type re-definition / "Flux" behavior)
    // This creates a local shadow in the current scope
    void redefine(const std::string& name, const std::string& newType, const Value& value) {
        values[name] = value;
        types[name] = newType;
        constFlags[name] = false;
    }

    // Get the environment where a variable is defined
    std::shared_ptr<Environment> findScope(const std::string& name) {
        if (values.count(name)) return shared_from_this();
        if (parent) return parent->findScope(name);
        return nullptr;
    }

private:
    std::unordered_map<std::string, Value> values;
    std::unordered_map<std::string, std::string> types;
    std::unordered_map<std::string, bool> constFlags;
};
