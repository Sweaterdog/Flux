#include "std_sys.h"
#include "../src/interpreter.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <csignal>
#include <functional>
#include <map>

// ============================================================================
// std.sys — Threading, mutex, atomic, signal handling, and system utilities
// ============================================================================

// Global interpreter mutex for thread safety
static std::mutex g_interpMutex;

// Signal handler storage
static std::map<int, std::function<void()>> g_signalHandlers;
static Interpreter* g_signalInterp = nullptr;

static void fluxSignalHandler(int signum) {
    auto it = g_signalHandlers.find(signum);
    if (it != g_signalHandlers.end()) {
        it->second();
    }
}

void registerStdSys(std::shared_ptr<Environment> env, Interpreter& interp) {

    // Store interpreter reference for signal handling
    g_signalInterp = &interp;

    // Create "thread" namespace object
    auto threadClass = std::make_shared<FluxClass>();
    threadClass->name = "thread";
    auto threadObj = std::make_shared<FluxObject>();
    threadObj->classDef = threadClass;

    // ---------- thread.run(func, args...) -> thread handle ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty() || args[0].type != ValueType::FUNCTION) {
                throw FluxException("error", "thread.run requires a function argument");
            }

            auto func = args[0].functionVal;
            std::vector<Value> funcArgs(args.begin() + 1, args.end());

            // Create a thread handle object
            auto handleClass = std::make_shared<FluxClass>();
            handleClass->name = "ThreadHandle";
            auto handleObj = std::make_shared<FluxObject>();
            handleObj->classDef = handleClass;

            // Shared result storage
            auto threadResult = std::make_shared<Value>(Value::nil());
            auto threadError = std::make_shared<std::string>("");

            // Launch the thread with interpreter mutex for safety
            auto threadPtr = std::make_shared<std::thread>([func, funcArgs, &interp, threadResult, threadError]() {
                try {
                    // Lock the interpreter mutex for thread-safe access
                    std::lock_guard<std::mutex> lock(g_interpMutex);
                    *threadResult = interp.invokeFunction(func, funcArgs);
                } catch (const ReturnSignal& rs) {
                    *threadResult = rs.value;
                } catch (const std::exception& e) {
                    *threadError = e.what();
                    std::cerr << "[Thread Error] " << e.what() << std::endl;
                }
            });

            // join()
            {
                Value joinFn;
                joinFn.type = ValueType::NATIVE_FUNCTION;
                joinFn.nativeFn = [threadPtr](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (threadPtr && threadPtr->joinable()) {
                        threadPtr->join();
                    }
                    return Value::nil();
                };
                handleObj->fields["join"] = joinFn;
            }

            // result() -> value returned by the thread function
            {
                Value resultFn;
                resultFn.type = ValueType::NATIVE_FUNCTION;
                resultFn.nativeFn = [threadResult](Interpreter& interp, std::vector<Value> args) -> Value {
                    return *threadResult;
                };
                handleObj->fields["result"] = resultFn;
            }

            // error() -> string error message (empty if no error)
            {
                Value errFn;
                errFn.type = ValueType::NATIVE_FUNCTION;
                errFn.nativeFn = [threadError](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromString(*threadError);
                };
                handleObj->fields["error"] = errFn;
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = handleObj;
            return result;
        };
        threadObj->fields["run"] = fn;
    }

    // ---------- thread.sleep(ms) ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::nil();
            int ms = (int)args[0].toNumber();
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return Value::nil();
        };
        threadObj->fields["sleep"] = fn;
    }

    Value threadVal;
    threadVal.type = ValueType::OBJECT;
    threadVal.objectVal = threadObj;
    env->define("thread", threadVal, "object");

    // ========================================================================
    // Mutex constructor
    // ========================================================================
    {
        Value mutexCtor;
        mutexCtor.type = ValueType::NATIVE_FUNCTION;
        mutexCtor.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            auto mutexClass = std::make_shared<FluxClass>();
            mutexClass->name = "Mutex";
            auto mutexObj = std::make_shared<FluxObject>();
            mutexObj->classDef = mutexClass;

            auto mtx = std::make_shared<std::mutex>();

            // lock()
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [mtx](Interpreter& interp, std::vector<Value> args) -> Value {
                    mtx->lock();
                    return Value::nil();
                };
                mutexObj->fields["lock"] = fn;
            }

            // unlock()
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [mtx](Interpreter& interp, std::vector<Value> args) -> Value {
                    mtx->unlock();
                    return Value::nil();
                };
                mutexObj->fields["unlock"] = fn;
            }

            // tryLock() -> bool
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [mtx](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromBool(mtx->try_lock());
                };
                mutexObj->fields["tryLock"] = fn;
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = mutexObj;
            return result;
        };
        env->define("Mutex", mutexCtor, "native_function");
    }

    // ========================================================================
    // sys namespace — system utilities
    // ========================================================================
    auto sysClass = std::make_shared<FluxClass>();
    sysClass->name = "sys";
    auto sysObj = std::make_shared<FluxObject>();
    sysObj->classDef = sysClass;

    // sys.time() -> long (Unix timestamp in milliseconds)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            return Value::fromLong(ms);
        };
        sysObj->fields["time"] = fn;
    }

    // sys.env(key) -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromString("");
            const char* val = std::getenv(args[0].toString().c_str());
            if (!val) return Value::fromString("");
            return Value::fromString(std::string(val));
        };
        sysObj->fields["env"] = fn;
    }

    // sys.exit(code)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            int code = args.empty() ? 0 : (int)args[0].toNumber();
            std::exit(code);
            return Value::nil();
        };
        sysObj->fields["exit"] = fn;
    }

    // sys.platform -> string
    {
#ifdef __linux__
        sysObj->fields["platform"] = Value::fromString("linux");
#elif __APPLE__
        sysObj->fields["platform"] = Value::fromString("macos");
#elif _WIN32
        sysObj->fields["platform"] = Value::fromString("windows");
#else
        sysObj->fields["platform"] = Value::fromString("unknown");
#endif
    }

    // sys.arch -> string
    {
#ifdef __x86_64__
        sysObj->fields["arch"] = Value::fromString("x86_64");
#elif __aarch64__
        sysObj->fields["arch"] = Value::fromString("aarch64");
#elif __arm__
        sysObj->fields["arch"] = Value::fromString("arm");
#else
        sysObj->fields["arch"] = Value::fromString("unknown");
#endif
    }

    // sys.args -> List<string> (populated at startup, initially empty)
    {
        auto argsList = std::make_shared<std::vector<Value>>();
        Value argsVal;
        argsVal.type = ValueType::LIST;
        argsVal.listVal = argsList;
        sysObj->fields["args"] = argsVal;
    }

    // sys.cpuCount() -> int (number of hardware threads)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            unsigned int n = std::thread::hardware_concurrency();
            return Value::fromInt(n > 0 ? (int)n : 1);
        };
        sysObj->fields["cpuCount"] = fn;
    }

    Value sysVal;
    sysVal.type = ValueType::OBJECT;
    sysVal.objectVal = sysObj;
    env->define("sys", sysVal, "object");

    // ========================================================================
    // Signal — interrupt and signal handler API
    //   Signal.handle(signum, func)  Register a handler
    //   Signal.raise(signum)         Send a signal to self
    //   Signal.ignore(signum)        Ignore a signal
    //   Signal.reset(signum)         Reset to default handler
    //   Signal.SIGINT, SIGTERM, SIGHUP, SIGUSR1, SIGUSR2
    // ========================================================================
    auto sigClass = std::make_shared<FluxClass>();
    sigClass->name = "Signal";
    auto sigObj = std::make_shared<FluxObject>();
    sigObj->classDef = sigClass;

    // Signal constants
    sigObj->fields["SIGINT"]  = Value::fromInt(SIGINT);
    sigObj->fields["SIGTERM"] = Value::fromInt(SIGTERM);
#ifndef _WIN32
    sigObj->fields["SIGHUP"]  = Value::fromInt(SIGHUP);
    sigObj->fields["SIGUSR1"] = Value::fromInt(SIGUSR1);
    sigObj->fields["SIGUSR2"] = Value::fromInt(SIGUSR2);
    sigObj->fields["SIGPIPE"] = Value::fromInt(SIGPIPE);
    sigObj->fields["SIGALRM"] = Value::fromInt(SIGALRM);
    sigObj->fields["SIGCHLD"] = Value::fromInt(SIGCHLD);
#endif
    sigObj->fields["SIGABRT"] = Value::fromInt(SIGABRT);
    sigObj->fields["SIGFPE"]  = Value::fromInt(SIGFPE);
    sigObj->fields["SIGSEGV"] = Value::fromInt(SIGSEGV);

    // Signal.handle(signum, func)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [&interp](Interpreter& unusedInterp, std::vector<Value> args) -> Value {
            if (args.size() < 2) throw FluxException("error", "Signal.handle requires signal number and function");
            int signum = (int)args[0].toNumber();
            if (args[1].type != ValueType::FUNCTION) {
                throw FluxException("error", "Signal.handle second argument must be a function");
            }
            auto func = args[1].functionVal;
            // Store a C++ callable that invokes the Flux function
            g_signalHandlers[signum] = [func, &interp]() {
                try {
                    std::lock_guard<std::mutex> lock(g_interpMutex);
                    interp.invokeFunction(func, {});
                } catch (...) {
                    // Signal handlers must not throw
                }
            };
            std::signal(signum, fluxSignalHandler);
            return Value::nil();
        };
        sigObj->fields["handle"] = fn;
    }

    // Signal.raise(signum)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Signal.raise requires a signal number");
            int signum = (int)args[0].toNumber();
            std::raise(signum);
            return Value::nil();
        };
        sigObj->fields["raise"] = fn;
    }

    // Signal.ignore(signum)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Signal.ignore requires a signal number");
            int signum = (int)args[0].toNumber();
            std::signal(signum, SIG_IGN);
            g_signalHandlers.erase(signum);
            return Value::nil();
        };
        sigObj->fields["ignore"] = fn;
    }

    // Signal.reset(signum)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Signal.reset requires a signal number");
            int signum = (int)args[0].toNumber();
            std::signal(signum, SIG_DFL);
            g_signalHandlers.erase(signum);
            return Value::nil();
        };
        sigObj->fields["reset"] = fn;
    }

    Value sigVal;
    sigVal.type = ValueType::OBJECT;
    sigVal.objectVal = sigObj;
    env->define("Signal", sigVal, "object");
}
