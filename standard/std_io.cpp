#include "std_io.h"
#include "../src/interpreter.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace fs_impl = std::filesystem;

// ============================================================================
// std.io — File system operations
// ============================================================================

void registerStdIO(std::shared_ptr<Environment> env, Interpreter& interp) {
    // Create the "fs" namespace object
    auto fsClass = std::make_shared<FluxClass>();
    fsClass->name = "fs";
    auto fsObj = std::make_shared<FluxObject>();
    fsObj->classDef = fsClass;

    // ---------- fs.read(path) -> string ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("FileSystemError", "fs.read requires a path argument");
            std::string path = args[0].toString();
            std::ifstream file(path);
            if (!file.is_open()) {
                throw FluxException("FileSystemError", "Cannot open file: " + path);
            }
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            file.close();
            return Value::fromString(content);
        };
        fsObj->fields["read"] = fn;
    }

    // ---------- fs.write(path, data) -> void ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.size() < 2) throw FluxException("FileSystemError", "fs.write requires path and data");
            std::string path = args[0].toString();
            std::string data = args[1].toString();
            std::ofstream file(path, std::ios::trunc);
            if (!file.is_open()) {
                throw FluxException("FileSystemError", "Cannot write to file: " + path);
            }
            file << data;
            file.close();
            return Value::nil();
        };
        fsObj->fields["write"] = fn;
    }

    // ---------- fs.append(path, data) -> void ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.size() < 2) throw FluxException("FileSystemError", "fs.append requires path and data");
            std::string path = args[0].toString();
            std::string data = args[1].toString();
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) {
                throw FluxException("FileSystemError", "Cannot append to file: " + path);
            }
            file << data;
            file.close();
            return Value::nil();
        };
        fsObj->fields["append"] = fn;
    }

    // ---------- fs.exists(path) -> bool ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromBool(false);
            return Value::fromBool(fs_impl::exists(args[0].toString()));
        };
        fsObj->fields["exists"] = fn;
    }

    // ---------- fs.delete(path) -> void ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("FileSystemError", "fs.delete requires a path");
            std::string path = args[0].toString();
            if (!fs_impl::exists(path)) {
                throw FluxException("FileSystemError", "File not found: " + path);
            }
            fs_impl::remove(path);
            return Value::nil();
        };
        // "delete" is a C++ keyword for field names but fine as a string key
        fsObj->fields["delete"] = fn;  // Accessible as fs.delete in Flux
        fsObj->fields["remove"] = fn;  // Also accessible as fs.remove
    }

    // ---------- fs.list(dir) -> List<string> ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("FileSystemError", "fs.list requires a directory path");
            std::string dirPath = args[0].toString();
            if (!fs_impl::is_directory(dirPath)) {
                throw FluxException("FileSystemError", "Not a directory: " + dirPath);
            }
            auto list = std::make_shared<std::vector<Value>>();
            for (auto& entry : fs_impl::directory_iterator(dirPath)) {
                list->push_back(Value::fromString(entry.path().filename().string()));
            }
            Value result;
            result.type = ValueType::LIST;
            result.listVal = list;
            return result;
        };
        fsObj->fields["list"] = fn;
    }

    // ---------- fs.mkdir(path) -> void ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("FileSystemError", "fs.mkdir requires a path");
            fs_impl::create_directories(args[0].toString());
            return Value::nil();
        };
        fsObj->fields["mkdir"] = fn;
    }

    // ---------- fs.size(path) -> int ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("FileSystemError", "fs.size requires a path");
            std::string path = args[0].toString();
            if (!fs_impl::exists(path)) {
                throw FluxException("FileSystemError", "File not found: " + path);
            }
            return Value::fromLong((int64_t)fs_impl::file_size(path));
        };
        fsObj->fields["size"] = fn;
    }

    // ---------- fs.copy(src, dst) -> void ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.size() < 2) throw FluxException("FileSystemError", "fs.copy requires src and dst paths");
            fs_impl::copy(args[0].toString(), args[1].toString(),
                          fs_impl::copy_options::overwrite_existing);
            return Value::nil();
        };
        fsObj->fields["copy"] = fn;
    }

    // ---------- fs.rename(oldPath, newPath) -> void ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.size() < 2) throw FluxException("FileSystemError", "fs.rename requires old and new paths");
            fs_impl::rename(args[0].toString(), args[1].toString());
            return Value::nil();
        };
        fsObj->fields["rename"] = fn;
    }

    Value fsVal;
    fsVal.type = ValueType::OBJECT;
    fsVal.objectVal = fsObj;
    env->define("fs", fsVal, "object");
}
