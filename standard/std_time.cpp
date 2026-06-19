#include "std_time.h"
#include "../src/interpreter.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// ============================================================================
// std.time — Date/time utilities
//
// Provides:
//   Time.now()             -> int (Unix timestamp in seconds)
//   Time.nowMs()           -> long (Unix timestamp in milliseconds)
//   Time.format(ts, fmt)   -> string (strftime format)
//   Time.parse(str, fmt)   -> int (parse string to timestamp)
//   Time.year(ts)          -> int
//   Time.month(ts)         -> int (1-12)
//   Time.day(ts)           -> int (1-31)
//   Time.hour(ts)          -> int (0-23)
//   Time.minute(ts)        -> int (0-59)
//   Time.second(ts)        -> int (0-59)
//   Time.dayOfWeek(ts)     -> int (0=Sun, 6=Sat)
//   Time.elapsed(start)    -> float (seconds since start timestamp)
//   Timer()                -> timer object with start()/stop()/elapsed()
// ============================================================================

void registerStdTime(std::shared_ptr<Environment> env, Interpreter& interp) {

    auto timeClass = std::make_shared<FluxClass>();
    timeClass->name = "Time";
    auto timeObj = std::make_shared<FluxObject>();
    timeObj->classDef = timeClass;

    // Time.now() -> int (seconds since epoch)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            auto now = std::chrono::system_clock::now();
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();
            return Value::fromFloat((double)secs);
        };
        timeObj->fields["now"] = fn;
    }

    // Time.nowMs() -> long (milliseconds since epoch)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            return Value::fromLong((int64_t)ms);
        };
        timeObj->fields["nowMs"] = fn;
    }

    // Time.format(timestamp, format_string) -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.size() < 2) {
                throw FluxException("error", "Time.format requires (timestamp, format_string)");
            }
            time_t ts = (time_t)args[0].toNumber();
            std::string fmt = args[1].toString();
            struct tm* tm_info = localtime(&ts);
            char buf[256];
            strftime(buf, sizeof(buf), fmt.c_str(), tm_info);
            return Value::fromString(std::string(buf));
        };
        timeObj->fields["format"] = fn;
    }

    // Time.parse(str, fmt) -> int (timestamp)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.size() < 2) {
                throw FluxException("error", "Time.parse requires (string, format_string)");
            }
            std::string input = args[0].toString();
            std::string fmt = args[1].toString();
            struct tm tm_info = {};
            std::istringstream iss(input);
            iss >> std::get_time(&tm_info, fmt.c_str());
            if (iss.fail()) {
                throw FluxException("error", "Time.parse failed to parse '" + input + "' with format '" + fmt + "'");
            }
            time_t ts = mktime(&tm_info);
            return Value::fromInt((int32_t)ts);
        };
        timeObj->fields["parse"] = fn;
    }

    // Helper lambda for extracting struct tm fields
    auto makeTimePart = [](std::function<int(struct tm*)> extractor) -> Value {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [extractor](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Time field accessor requires a timestamp argument");
            time_t ts = (time_t)args[0].toNumber();
            struct tm* tm_info = localtime(&ts);
            return Value::fromInt(extractor(tm_info));
        };
        return fn;
    };

    timeObj->fields["year"]      = makeTimePart([](struct tm* t) { return t->tm_year + 1900; });
    timeObj->fields["month"]     = makeTimePart([](struct tm* t) { return t->tm_mon + 1; });
    timeObj->fields["day"]       = makeTimePart([](struct tm* t) { return t->tm_mday; });
    timeObj->fields["hour"]      = makeTimePart([](struct tm* t) { return t->tm_hour; });
    timeObj->fields["minute"]    = makeTimePart([](struct tm* t) { return t->tm_min; });
    timeObj->fields["second"]    = makeTimePart([](struct tm* t) { return t->tm_sec; });
    timeObj->fields["dayOfWeek"] = makeTimePart([](struct tm* t) { return t->tm_wday; });

    // Time.elapsed(start_ms) -> float (seconds since start)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Time.elapsed requires a start timestamp (ms)");
            int64_t startMs = (int64_t)args[0].toNumber();
            auto now = std::chrono::system_clock::now();
            auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            double elapsed = (double)(nowMs - startMs) / 1000.0;
            return Value::fromFloat(elapsed);
        };
        timeObj->fields["elapsed"] = fn;
    }

    Value timeVal;
    timeVal.type = ValueType::OBJECT;
    timeVal.objectVal = timeObj;
    env->define("Time", timeVal, "object");

    // ========================================================================
    // Timer() constructor — returns an object with start()/stop()/elapsed()
    // ========================================================================
    {
        Value timerCtor;
        timerCtor.type = ValueType::NATIVE_FUNCTION;
        timerCtor.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            auto cls = std::make_shared<FluxClass>();
            cls->name = "Timer";
            auto obj = std::make_shared<FluxObject>();
            obj->classDef = cls;

            auto startTime = std::make_shared<std::chrono::high_resolution_clock::time_point>();
            auto stopTime = std::make_shared<std::chrono::high_resolution_clock::time_point>();
            auto running = std::make_shared<bool>(false);

            // start()
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [startTime, running](Interpreter&, std::vector<Value>) -> Value {
                    *startTime = std::chrono::high_resolution_clock::now();
                    *running = true;
                    return Value::nil();
                };
                obj->fields["start"] = fn;
            }

            // stop()
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [stopTime, running](Interpreter&, std::vector<Value>) -> Value {
                    *stopTime = std::chrono::high_resolution_clock::now();
                    *running = false;
                    return Value::nil();
                };
                obj->fields["stop"] = fn;
            }

            // elapsed() -> float (seconds)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [startTime, stopTime, running](Interpreter&, std::vector<Value>) -> Value {
                    auto end = *running ? std::chrono::high_resolution_clock::now() : *stopTime;
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - *startTime);
                    return Value::fromFloat(duration.count() / 1e6);
                };
                obj->fields["elapsed"] = fn;
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = obj;
            return result;
        };
        env->define("Timer", timerCtor, "native_function");
    }
}
