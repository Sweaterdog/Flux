#include "std_os.h"
#include "../src/interpreter.h"
#include <cstdlib>
#include <cstdio>
#include <array>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>

// ============================================================================
// std.os — Operating system interaction
//
// Provides:
//   OS.exec(command)       -> string (stdout output)
//   OS.execStatus(command) -> int (exit code)
//   OS.env(key)            -> string (environment variable)
//   OS.setEnv(key, val)    -> nil
//   OS.cwd()               -> string (current working directory)
//   OS.chdir(path)         -> nil
//   OS.pid()               -> int
//   OS.hostname()          -> string
//   OS.username()          -> string
//   OS.tempDir()           -> string
//   OS.args                -> list (populated by interpreter from argv)
// ============================================================================

void registerStdOS(std::shared_ptr<Environment> env, Interpreter& interp) {

    auto osClass = std::make_shared<FluxClass>();
    osClass->name = "OS";
    auto osObj = std::make_shared<FluxObject>();
    osObj->classDef = osClass;

    // OS.exec(command) -> string (captured stdout)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "OS.exec requires a command string");
            std::string cmd = args[0].toString();

            std::array<char, 4096> buffer;
            std::string result;
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                throw FluxException("error", "OS.exec failed to open pipe for: " + cmd);
            }
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
            }
            int status = pclose(pipe);
            (void)status;
            return Value::fromString(result);
        };
        osObj->fields["exec"] = fn;
    }

    // OS.execStatus(command) -> int (exit code)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "OS.execStatus requires a command string");
            std::string cmd = args[0].toString();
            int status = std::system(cmd.c_str());
            int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            return Value::fromInt(exitCode);
        };
        osObj->fields["execStatus"] = fn;
    }

    // OS.env(key) -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromString("");
            const char* val = std::getenv(args[0].toString().c_str());
            return Value::fromString(val ? std::string(val) : "");
        };
        osObj->fields["env"] = fn;
    }

    // OS.setEnv(key, value) -> nil
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.size() < 2) throw FluxException("error", "OS.setEnv requires (key, value)");
            setenv(args[0].toString().c_str(), args[1].toString().c_str(), 1);
            return Value::nil();
        };
        osObj->fields["setEnv"] = fn;
    }

    // OS.cwd() -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            return Value::fromString(std::filesystem::current_path().string());
        };
        osObj->fields["cwd"] = fn;
    }

    // OS.chdir(path) -> nil
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "OS.chdir requires a path argument");
            std::filesystem::current_path(args[0].toString());
            return Value::nil();
        };
        osObj->fields["chdir"] = fn;
    }

    // OS.pid() -> int
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            return Value::fromInt((int32_t)getpid());
        };
        osObj->fields["pid"] = fn;
    }

    // OS.hostname() -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            char buf[256];
            if (gethostname(buf, sizeof(buf)) == 0) {
                return Value::fromString(std::string(buf));
            }
            return Value::fromString("unknown");
        };
        osObj->fields["hostname"] = fn;
    }

    // OS.username() -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            const char* user = std::getenv("USER");
            if (!user) user = std::getenv("LOGNAME");
            return Value::fromString(user ? std::string(user) : "unknown");
        };
        osObj->fields["username"] = fn;
    }

    // OS.tempDir() -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            return Value::fromString(std::filesystem::temp_directory_path().string());
        };
        osObj->fields["tempDir"] = fn;
    }

    // OS.platform -> string (same as sys.platform but accessible here too)
    {
#ifdef __linux__
        osObj->fields["platform"] = Value::fromString("linux");
#elif __APPLE__
        osObj->fields["platform"] = Value::fromString("macos");
#elif _WIN32
        osObj->fields["platform"] = Value::fromString("windows");
#else
        osObj->fields["platform"] = Value::fromString("unknown");
#endif
    }

    Value osVal;
    osVal.type = ValueType::OBJECT;
    osVal.objectVal = osObj;
    env->define("OS", osVal, "object");
}
