#include "std_collections.h"
#include "../src/interpreter.h"
#include <map>
#include <deque>

// ============================================================================
// std.collections — Map, Stack, Queue constructors
// ============================================================================

void registerStdCollections(std::shared_ptr<Environment> env, Interpreter& interp) {

    // ========================================================================
    // Map() — Hash map (key-value store)
    //   .put(key, value)     Insert or update
    //   .get(key) -> V       Get value (throws if not found)
    //   .hasKey(key) -> bool Check if key exists
    //   .remove(key)         Remove key
    //   .keys() -> List      Get all keys
    //   .values() -> List    Get all values
    //   .length -> int       Number of entries
    // ========================================================================
    {
        Value mapCtor;
        mapCtor.type = ValueType::NATIVE_FUNCTION;
        mapCtor.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            auto mapClass = std::make_shared<FluxClass>();
            mapClass->name = "Map";
            auto mapObj = std::make_shared<FluxObject>();
            mapObj->classDef = mapClass;

            // Internal storage: use a shared ordered map of string->Value
            auto store = std::make_shared<std::map<std::string, Value>>();

            // put(key, value)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.size() < 2) throw FluxException("error", "Map.put requires key and value");
                    (*store)[args[0].toString()] = args[1];
                    return Value::nil();
                };
                mapObj->fields["put"] = fn;
            }

            // get(key) -> Value
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) throw FluxException("error", "Map.get requires a key");
                    std::string key = args[0].toString();
                    auto it = store->find(key);
                    if (it == store->end()) {
                        throw FluxException("IndexError", "Key not found in Map: " + key);
                    }
                    return it->second;
                };
                mapObj->fields["get"] = fn;
            }

            // hasKey(key) -> bool
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) return Value::fromBool(false);
                    return Value::fromBool(store->count(args[0].toString()) > 0);
                };
                mapObj->fields["hasKey"] = fn;
            }

            // remove(key)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) return Value::nil();
                    store->erase(args[0].toString());
                    return Value::nil();
                };
                mapObj->fields["remove"] = fn;
            }

            // keys() -> List<string>
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    auto list = std::make_shared<std::vector<Value>>();
                    for (auto& [k, v] : *store) {
                        list->push_back(Value::fromString(k));
                    }
                    Value result;
                    result.type = ValueType::LIST;
                    result.listVal = list;
                    return result;
                };
                mapObj->fields["keys"] = fn;
            }

            // values() -> List
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    auto list = std::make_shared<std::vector<Value>>();
                    for (auto& [k, v] : *store) {
                        list->push_back(v);
                    }
                    Value result;
                    result.type = ValueType::LIST;
                    result.listVal = list;
                    return result;
                };
                mapObj->fields["values"] = fn;
            }

            // length
            // length — dynamic, returns current size
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromInt((int)store->size());
                };
                mapObj->fields["length"] = fn;
            }

            // size() — same as length, for consistency
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromInt((int)store->size());
                };
                mapObj->fields["size"] = fn;
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = mapObj;
            return result;
        };
        env->define("Map", mapCtor, "native_function");
    }

    // ========================================================================
    // Stack() — LIFO stack
    //   .push(item)      Add to top
    //   .pop() -> T      Remove and return from top
    //   .peek() -> T     View top without removing
    //   .length -> int   Number of elements
    //   .isEmpty() -> bool
    // ========================================================================
    {
        Value stackCtor;
        stackCtor.type = ValueType::NATIVE_FUNCTION;
        stackCtor.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            auto stackClass = std::make_shared<FluxClass>();
            stackClass->name = "Stack";
            auto stackObj = std::make_shared<FluxObject>();
            stackObj->classDef = stackClass;

            auto store = std::make_shared<std::vector<Value>>();

            // push(item)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (!args.empty()) store->push_back(args[0]);
                    return Value::nil();
                };
                stackObj->fields["push"] = fn;
            }

            // pop() -> T
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (store->empty()) throw FluxException("IndexError", "Stack is empty");
                    Value top = store->back();
                    store->pop_back();
                    return top;
                };
                stackObj->fields["pop"] = fn;
            }

            // peek() -> T
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (store->empty()) throw FluxException("IndexError", "Stack is empty");
                    return store->back();
                };
                stackObj->fields["peek"] = fn;
            }

            // size() -> int
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromInt((int)store->size());
                };
                stackObj->fields["size"] = fn;
            }

            // isEmpty() -> bool
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromBool(store->empty());
                };
                stackObj->fields["isEmpty"] = fn;
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = stackObj;
            return result;
        };
        env->define("Stack", stackCtor, "native_function");
    }

    // ========================================================================
    // Queue() — FIFO queue
    //   .enqueue(item)      Add to back
    //   .dequeue() -> T     Remove from front
    //   .peek() -> T        View front without removing
    //   .size() -> int      Number of elements
    //   .isEmpty() -> bool
    // ========================================================================
    {
        Value queueCtor;
        queueCtor.type = ValueType::NATIVE_FUNCTION;
        queueCtor.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            auto queueClass = std::make_shared<FluxClass>();
            queueClass->name = "Queue";
            auto queueObj = std::make_shared<FluxObject>();
            queueObj->classDef = queueClass;

            auto store = std::make_shared<std::deque<Value>>();

            // enqueue(item)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (!args.empty()) store->push_back(args[0]);
                    return Value::nil();
                };
                queueObj->fields["enqueue"] = fn;
            }

            // dequeue() -> T
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (store->empty()) throw FluxException("IndexError", "Queue is empty");
                    Value front = store->front();
                    store->pop_front();
                    return front;
                };
                queueObj->fields["dequeue"] = fn;
            }

            // peek() -> T
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (store->empty()) throw FluxException("IndexError", "Queue is empty");
                    return store->front();
                };
                queueObj->fields["peek"] = fn;
            }

            // size() -> int
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromInt((int)store->size());
                };
                queueObj->fields["size"] = fn;
            }

            // isEmpty() -> bool
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [store](Interpreter& interp, std::vector<Value> args) -> Value {
                    return Value::fromBool(store->empty());
                };
                queueObj->fields["isEmpty"] = fn;
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = queueObj;
            return result;
        };
        env->define("Queue", queueCtor, "native_function");
    }
}
