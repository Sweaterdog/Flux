#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstdlib>
#include <set>

// Standard library modules
#include "../standard/std_io.h"
#include "../standard/std_net.h"
#include "../standard/std_collections.h"
#include "../standard/std_sys.h"
#include "../standard/std_json.h"
#include "../standard/std_time.h"
#include "../standard/std_crypto.h"
#include "../standard/std_os.h"
#include "../standard/std_regex.h"
#include "../standard/std_gpu.h"
#include "../standard/std_graphics.h"
#include "../standard/std_audio.h"
#include "../standard/std_video.h"

// ============================================================================
// Interpreter Implementation
// ============================================================================

Interpreter::Interpreter() {
    globalEnv = std::make_shared<Environment>();
    currentEnv = globalEnv;
    registerBuiltins();
}

// ============================================================================
// Built-in functions and types registration
// ============================================================================

void Interpreter::registerBuiltins() {
    // ---- print(value) ----
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout << std::endl;
            return Value::nil();
        };
        globalEnv->define("print", fn, "native_function");
    }

    // ---- print_raw(value) ----
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            for (size_t i = 0; i < args.size(); i++) {
                std::cout << args[i].toString();
            }
            std::cout << std::flush;
            return Value::nil();
        };
        globalEnv->define("print_raw", fn, "native_function");
    }

    // ---- input(prompt) -> string ----
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (!args.empty()) {
                std::cout << args[0].toString();
                std::cout << std::flush;
            }
            std::string line;
            std::getline(std::cin, line);
            return Value::fromString(line);
        };
        globalEnv->define("input", fn, "native_function");
    }

    // ---- typeof(value) -> string ----
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromString("void");
            return Value::fromString(args[0].typeName());
        };
        globalEnv->define("typeof", fn, "native_function");
    }

    // ---- len(value) -> int ----
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromInt(0);
            const Value& v = args[0];
            if (v.type == ValueType::STRING) return Value::fromInt((int)v.stringVal.size());
            if (v.type == ValueType::LIST && v.listVal) return Value::fromInt((int)v.listVal->size());
            return Value::fromInt(0);
        };
        globalEnv->define("len", fn, "native_function");
    }

    // ---- toString(value) -> string ----
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromString("");
            return Value::fromString(args[0].toString());
        };
        globalEnv->define("toString", fn, "native_function");
    }

    // ---- math object (as a map-like namespace) ----
    registerMathBuiltins();
}

// ============================================================================
// Math builtins - registered under the "math" namespace variable
// ============================================================================

void Interpreter::registerMathBuiltins() {
    // We'll register math functions as top-level for now and also
    // create a "math" object with these methods for std.math style access

    // math.sqrt
    auto makeMathFn1 = [&](const std::string& name, std::function<double(double)> impl) {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [impl](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromFloat(0.0);
            return Value::fromFloat(impl(args[0].toNumber()));
        };
        return fn;
    };

    auto makeMathFn2 = [&](const std::string& name,
                           std::function<double(double, double)> impl) {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [impl](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.size() < 2) return Value::fromFloat(0.0);
            return Value::fromFloat(impl(args[0].toNumber(), args[1].toNumber()));
        };
        return fn;
    };

    // Create the math object as a FluxObject
    auto mathClass = std::make_shared<FluxClass>();
    mathClass->name = "math";

    auto mathObj = std::make_shared<FluxObject>();
    mathObj->classDef = mathClass;

    // Single-arg math functions
    mathObj->fields["sqrt"] = makeMathFn1("sqrt", [](double x) { return std::sqrt(x); });
    mathObj->fields["abs"] = makeMathFn1("abs", [](double x) { return std::abs(x); });
    mathObj->fields["floor"] = makeMathFn1("floor", [](double x) { return std::floor(x); });
    mathObj->fields["ceil"] = makeMathFn1("ceil", [](double x) { return std::ceil(x); });
    mathObj->fields["round"] = makeMathFn1("round", [](double x) { return std::round(x); });
    mathObj->fields["sin"] = makeMathFn1("sin", [](double x) { return std::sin(x); });
    mathObj->fields["cos"] = makeMathFn1("cos", [](double x) { return std::cos(x); });
    mathObj->fields["tan"] = makeMathFn1("tan", [](double x) { return std::tan(x); });
    mathObj->fields["asin"] = makeMathFn1("asin", [](double x) { return std::asin(x); });
    mathObj->fields["acos"] = makeMathFn1("acos", [](double x) { return std::acos(x); });
    mathObj->fields["atan"] = makeMathFn1("atan", [](double x) { return std::atan(x); });
    mathObj->fields["log"] = makeMathFn1("log", [](double x) { return std::log(x); });
    mathObj->fields["log2"] = makeMathFn1("log2", [](double x) { return std::log2(x); });
    mathObj->fields["log10"] = makeMathFn1("log10", [](double x) { return std::log10(x); });
    mathObj->fields["exp"] = makeMathFn1("exp", [](double x) { return std::exp(x); });

    // Two-arg math functions
    mathObj->fields["pow"] = makeMathFn2("pow", [](double a, double b) { return std::pow(a, b); });
    mathObj->fields["min"] = makeMathFn2("min", [](double a, double b) { return std::min(a, b); });
    mathObj->fields["max"] = makeMathFn2("max", [](double a, double b) { return std::max(a, b); });
    mathObj->fields["atan2"] = makeMathFn2("atan2", [](double y, double x) { return std::atan2(y, x); });

    // clamp(v, min, max)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.size() < 3) return Value::fromFloat(0.0);
            double v = args[0].toNumber();
            double lo = args[1].toNumber();
            double hi = args[2].toNumber();
            return Value::fromFloat(std::max(lo, std::min(v, hi)));
        };
        mathObj->fields["clamp"] = fn;
    }

    // lerp(a, b, t)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            if (args.size() < 3) return Value::fromFloat(0.0);
            double a = args[0].toNumber();
            double b = args[1].toNumber();
            double t = args[2].toNumber();
            return Value::fromFloat(a + (b - a) * t);
        };
        mathObj->fields["lerp"] = fn;
    }

    // Math constants
    mathObj->fields["PI"] = Value::fromFloat(M_PI);
    mathObj->fields["E"] = Value::fromFloat(M_E);
    mathObj->fields["TAU"] = Value::fromFloat(M_PI * 2.0);
    mathObj->fields["INF"] = Value::fromFloat(std::numeric_limits<double>::infinity());

    Value mathVal;
    mathVal.type = ValueType::OBJECT;
    mathVal.objectVal = mathObj;
    globalEnv->define("math", mathVal, "object");

    // Register .random on type names
    // int.random, float.random, bool.random
    // These are handled specially in evalMemberAccess
}

// ============================================================================
// Top-level execution
// ============================================================================

void Interpreter::execute(ASTNodePtr program) {
    if (!program) return;

    auto prog = std::dynamic_pointer_cast<ProgramNode>(program);
    if (!prog) {
        // Single statement
        execStatement(program);
        return;
    }

    // First pass: register all function and class declarations
    for (auto& decl : prog->declarations) {
        if (decl->nodeType == NodeType::FUNC_DECL) {
            execFuncDecl(std::static_pointer_cast<FuncDeclNode>(decl));
        } else if (decl->nodeType == NodeType::CLASS_DECL) {
            execClassDecl(std::static_pointer_cast<ClassDeclNode>(decl));
        } else if (decl->nodeType == NodeType::STRUCT_DECL) {
            execStructDecl(std::static_pointer_cast<StructDeclNode>(decl));
        } else if (decl->nodeType == NodeType::ENUM_DECL) {
            execEnumDecl(std::static_pointer_cast<EnumDeclNode>(decl));
        }
    }

    // Second pass: execute all non-declaration statements and variable decls
    for (auto& decl : prog->declarations) {
        if (decl->nodeType != NodeType::FUNC_DECL &&
            decl->nodeType != NodeType::CLASS_DECL &&
            decl->nodeType != NodeType::STRUCT_DECL &&
            decl->nodeType != NodeType::ENUM_DECL) {
            execStatement(decl);
        }
    }

    // Look for main() and call it if it exists
    if (currentEnv->has("main")) {
        Value mainVal = currentEnv->get("main");
        if (mainVal.type == ValueType::FUNCTION && mainVal.functionVal) {
            try {
                callFunction(mainVal.functionVal, {});
            } catch (const ReturnSignal& ret) {
                // main returned a value, ignore
            } catch (const PanicSignal& panic) {
                std::cerr << "PANIC: " << panic.message << std::endl;
                std::exit(1);
            } catch (const FluxException& e) {
                std::cerr << "Unhandled exception [" << e.errorType << "]: "
                          << e.message << std::endl;
                std::exit(1);
            }
        }
    }
}

// ============================================================================
// Statement execution
// ============================================================================

void Interpreter::execStatement(ASTNodePtr node) {
    if (!node) return;

    switch (node->nodeType) {
        case NodeType::EXPRESSION_STMT: {
            auto stmt = std::static_pointer_cast<ExpressionStmtNode>(node);
            eval(stmt->expression);
            break;
        }
        case NodeType::VAR_DECL:
            execVarDecl(std::static_pointer_cast<VarDeclNode>(node));
            break;
        case NodeType::BLOCK:
            execBlock(std::static_pointer_cast<BlockNode>(node),
                      std::make_shared<Environment>(currentEnv));
            break;
        case NodeType::IF_STMT:
            execIf(std::static_pointer_cast<IfStmtNode>(node));
            break;
        case NodeType::SWITCH_STMT:
            execSwitch(std::static_pointer_cast<SwitchStmtNode>(node));
            break;
        case NodeType::FOR_STMT:
            execFor(std::static_pointer_cast<ForStmtNode>(node));
            break;
        case NodeType::FOR_EACH_STMT:
            execForEach(std::static_pointer_cast<ForEachStmtNode>(node));
            break;
        case NodeType::WHILE_STMT:
            execWhile(std::static_pointer_cast<WhileStmtNode>(node));
            break;
        case NodeType::DO_WHILE_STMT:
            execDoWhile(std::static_pointer_cast<DoWhileStmtNode>(node));
            break;
        case NodeType::FUNC_DECL:
            execFuncDecl(std::static_pointer_cast<FuncDeclNode>(node));
            break;
        case NodeType::CLASS_DECL:
            execClassDecl(std::static_pointer_cast<ClassDeclNode>(node));
            break;
        case NodeType::STRUCT_DECL:
            execStructDecl(std::static_pointer_cast<StructDeclNode>(node));
            break;
        case NodeType::ENUM_DECL:
            execEnumDecl(std::static_pointer_cast<EnumDeclNode>(node));
            break;
        case NodeType::RETURN_STMT: {
            auto ret = std::static_pointer_cast<ReturnStmtNode>(node);
            Value val = ret->value ? eval(ret->value) : Value::nil();
            throw ReturnSignal(val);
        }
        case NodeType::BREAK_STMT:
            throw BreakSignal();
        case NodeType::CONTINUE_STMT:
            throw ContinueSignal();
        case NodeType::TRY_CATCH:
            execTryCatch(std::static_pointer_cast<TryCatchNode>(node));
            break;
        case NodeType::THROW_STMT: {
            auto throwNode = std::static_pointer_cast<ThrowStmtNode>(node);
            Value val = eval(throwNode->expr);
            throw FluxException("error", val.toString());
        }
        case NodeType::PANIC_STMT: {
            auto panicNode = std::static_pointer_cast<PanicStmtNode>(node);
            Value msg = eval(panicNode->message);
            throw PanicSignal(msg.toString());
        }
        case NodeType::UNSAFE_BLOCK: {
            auto unsafeNode = std::static_pointer_cast<UnsafeBlockNode>(node);
            bool prevUnsafe = inUnsafe;
            inUnsafe = true;
            execStatement(unsafeNode->body);
            inUnsafe = prevUnsafe;
            break;
        }
        case NodeType::CLEANUP_STMT:
            // In our interpreter, cleanup is a no-op (ARC simulation)
            break;
        case NodeType::IMPORT_STMT:
            execImport(std::static_pointer_cast<ImportStmtNode>(node));
            break;
        case NodeType::EXPORT_STMT: {
            auto exportNode = std::static_pointer_cast<ExportStmtNode>(node);
            if (exportNode->declaration) {
                execStatement(exportNode->declaration);
            }
            break;
        }
        case NodeType::INTERFACE_DECL:
            // Interface declarations are type-level, nothing to execute at runtime
            break;
        default:
            // Treat as expression
            eval(node);
            break;
    }
}

void Interpreter::execVarDecl(std::shared_ptr<VarDeclNode> node) {
    Value val;
    if (node->initializer) {
        val = eval(node->initializer);
    } else {
        val = defaultValueForType(node->typeName);
    }

    currentEnv->define(node->name, val, node->typeName, node->isConst);
}

void Interpreter::execBlock(std::shared_ptr<BlockNode> node,
                            std::shared_ptr<Environment> env) {
    auto prevEnv = currentEnv;
    currentEnv = env;

    try {
        for (auto& stmt : node->statements) {
            execStatement(stmt);
        }
    } catch (...) {
        currentEnv = prevEnv;
        throw;
    }

    currentEnv = prevEnv;
}

void Interpreter::execIf(std::shared_ptr<IfStmtNode> node) {
    Value cond = eval(node->condition);
    if (cond.isTruthy()) {
        execStatement(node->thenBranch);
        return;
    }

    for (auto& [elifCond, elifBody] : node->elifBranches) {
        Value ec = eval(elifCond);
        if (ec.isTruthy()) {
            execStatement(elifBody);
            return;
        }
    }

    if (node->elseBranch) {
        execStatement(node->elseBranch);
    }
}

void Interpreter::execSwitch(std::shared_ptr<SwitchStmtNode> node) {
    Value val = eval(node->expr);
    bool matched = false;
    bool falling = false;

    for (auto& clause : node->cases) {
        if (!falling && !clause.isDefault) {
            Value caseVal = eval(clause.value);
            if (val.identityEquals(caseVal)) {
                matched = true;
                falling = true;
            }
        }
        if (!falling && clause.isDefault && !matched) {
            falling = true;
        }

        if (falling) {
            try {
                for (auto& stmt : clause.body) {
                    execStatement(stmt);
                }
            } catch (const BreakSignal&) {
                return; // break exits the switch
            }
        }
    }

    // If no case matched and we haven't fallen through to default yet,
    // try the default case
    if (!matched && !falling) {
        for (auto& clause : node->cases) {
            if (clause.isDefault) {
                try {
                    for (auto& stmt : clause.body) {
                        execStatement(stmt);
                    }
                } catch (const BreakSignal&) {
                    return;
                }
                break;
            }
        }
    }
}

void Interpreter::execFor(std::shared_ptr<ForStmtNode> node) {
    auto loopEnv = std::make_shared<Environment>(currentEnv);
    auto prevEnv = currentEnv;
    currentEnv = loopEnv;

    try {
        // Initializer
        if (node->initializer) {
            execStatement(node->initializer);
        }

        while (true) {
            // Condition
            if (node->condition) {
                Value cond = eval(node->condition);
                if (!cond.isTruthy()) break;
            }

            // Body
            try {
                execStatement(node->body);
            } catch (const BreakSignal&) {
                break;
            } catch (const ContinueSignal&) {
                // Continue to increment
            }

            // Increment
            if (node->increment) {
                eval(node->increment);
            }
        }
    } catch (...) {
        currentEnv = prevEnv;
        throw;
    }

    currentEnv = prevEnv;
}

void Interpreter::execForEach(std::shared_ptr<ForEachStmtNode> node) {
    Value iterable = eval(node->iterable);

    if (iterable.type == ValueType::LIST && iterable.listVal) {
        for (size_t i = 0; i < iterable.listVal->size(); i++) {
            auto loopEnv = std::make_shared<Environment>(currentEnv);
            loopEnv->define(node->varName, (*iterable.listVal)[i], node->varType);

            auto prevEnv = currentEnv;
            currentEnv = loopEnv;

            try {
                execStatement(node->body);
            } catch (const BreakSignal&) {
                currentEnv = prevEnv;
                break;
            } catch (const ContinueSignal&) {
                // continue
            } catch (...) {
                currentEnv = prevEnv;
                throw;
            }

            currentEnv = prevEnv;
        }
    } else if (iterable.type == ValueType::STRING) {
        for (size_t i = 0; i < iterable.stringVal.size(); i++) {
            auto loopEnv = std::make_shared<Environment>(currentEnv);
            if (node->varType == "char") {
                loopEnv->define(node->varName, Value::fromChar(iterable.stringVal[i]), "char");
            } else {
                loopEnv->define(node->varName,
                               Value::fromString(std::string(1, iterable.stringVal[i])),
                               "string");
            }

            auto prevEnv = currentEnv;
            currentEnv = loopEnv;

            try {
                execStatement(node->body);
            } catch (const BreakSignal&) {
                currentEnv = prevEnv;
                break;
            } catch (const ContinueSignal&) {
                // continue
            } catch (...) {
                currentEnv = prevEnv;
                throw;
            }

            currentEnv = prevEnv;
        }
    } else {
        runtimeError(node->line, "Cannot iterate over value of type '" +
                     iterable.typeName() + "'");
    }
}

void Interpreter::execWhile(std::shared_ptr<WhileStmtNode> node) {
    while (true) {
        Value cond = eval(node->condition);
        if (!cond.isTruthy()) break;

        try {
            execStatement(node->body);
        } catch (const BreakSignal&) {
            break;
        } catch (const ContinueSignal&) {
            // continue
        }
    }
}

void Interpreter::execDoWhile(std::shared_ptr<DoWhileStmtNode> node) {
    do {
        try {
            execStatement(node->body);
        } catch (const BreakSignal&) {
            return;
        } catch (const ContinueSignal&) {
            // continue
        }

        Value cond = eval(node->condition);
        if (!cond.isTruthy()) break;
    } while (true);
}

void Interpreter::execFuncDecl(std::shared_ptr<FuncDeclNode> node) {
    auto func = std::make_shared<FluxFunction>();
    func->name = node->name;
    func->params = node->params;
    func->returnType = node->returnType;
    func->body = node->body;
    func->closure = currentEnv;
    func->isMethod = false;

    Value funcVal;
    funcVal.type = ValueType::FUNCTION;
    funcVal.functionVal = func;

    currentEnv->define(node->name, funcVal, "function");
}

void Interpreter::execClassDecl(std::shared_ptr<ClassDeclNode> node) {
    auto classDef = std::make_shared<FluxClass>();
    classDef->name = node->name;
    classDef->parentName = node->parentClass;
    classDef->interfaces = node->interfaces;

    // Resolve parent class if exists
    if (!node->parentClass.empty() && currentEnv->has(node->parentClass)) {
        Value parentVal = currentEnv->get(node->parentClass);
        if (parentVal.type == ValueType::CLASS_DEF && parentVal.classVal) {
            classDef->parent = parentVal.classVal;
            // Inherit fields
            classDef->fieldTypes = parentVal.classVal->fieldTypes;
            classDef->fieldDefaults = parentVal.classVal->fieldDefaults;
            classDef->fieldAccess = parentVal.classVal->fieldAccess;
            // Inherit methods
            classDef->methods = parentVal.classVal->methods;
        }
    }

    // Process members
    for (auto& member : node->members) {
        if (member.isField) {
            classDef->fieldTypes[member.fieldName] = member.fieldType;
            classDef->fieldAccess[member.fieldName] = member.accessModifier;
            if (member.fieldInit) {
                classDef->fieldDefaults[member.fieldName] = eval(member.fieldInit);
            } else {
                classDef->fieldDefaults[member.fieldName] =
                    defaultValueForType(member.fieldType);
            }
        } else {
            // Method
            auto funcNode = std::static_pointer_cast<FuncDeclNode>(member.method);
            auto func = std::make_shared<FluxFunction>();
            func->name = funcNode->name;
            func->params = funcNode->params;
            func->returnType = funcNode->returnType;
            func->body = funcNode->body;
            func->closure = currentEnv;
            func->isMethod = true;

            // Check if this is the constructor
            if (funcNode->name == "init") {
                func->isInitializer = true;
            }

            Value methodVal;
            methodVal.type = ValueType::FUNCTION;
            methodVal.functionVal = func;
            classDef->methods[funcNode->name] = methodVal;
        }
    }

    Value classVal;
    classVal.type = ValueType::CLASS_DEF;
    classVal.classVal = classDef;
    currentEnv->define(node->name, classVal, "class");
}

void Interpreter::execStructDecl(std::shared_ptr<StructDeclNode> node) {
    auto structDef = std::make_shared<FluxStruct>();
    structDef->name = node->name;

    for (auto& field : node->fields) {
        structDef->fields.push_back({field.name, field.typeName});
    }

    Value structVal;
    structVal.type = ValueType::STRUCT_DEF;
    structVal.structVal = structDef;
    currentEnv->define(node->name, structVal, "struct");
}

void Interpreter::execEnumDecl(std::shared_ptr<EnumDeclNode> node) {
    auto enumDef = std::make_shared<FluxEnum>();
    enumDef->name = node->name;

    for (auto& member : node->members) {
        enumDef->values[member.name] = member.value;
    }

    Value enumVal;
    enumVal.type = ValueType::ENUM_DEF;
    enumVal.enumVal = enumDef;
    currentEnv->define(node->name, enumVal, "enum");
}

void Interpreter::execTryCatch(std::shared_ptr<TryCatchNode> node) {
    try {
        execStatement(node->tryBody);
    } catch (const FluxException& e) {
        bool caught = false;
        for (auto& clause : node->catchClauses) {
            // Match "error" catches everything, or match specific type
            if (clause.errorType == "error" || clause.errorType == e.errorType) {
                auto catchEnv = std::make_shared<Environment>(currentEnv);

                // Create an error object with message, code, stack fields
                auto errObj = std::make_shared<FluxObject>();
                auto errClass = std::make_shared<FluxClass>();
                errClass->name = e.errorType;
                errObj->classDef = errClass;
                errObj->fields["message"] = Value::fromString(e.message);
                errObj->fields["code"] = Value::fromInt(e.code);
                errObj->fields["stack"] = Value::fromString(e.stack);

                Value errVal;
                errVal.type = ValueType::OBJECT;
                errVal.objectVal = errObj;
                catchEnv->define(clause.errorName, errVal, clause.errorType);

                execBlock(std::static_pointer_cast<BlockNode>(clause.body), catchEnv);
                caught = true;
                break;
            }
        }
        if (!caught) {
            // Execute finally if present, then re-throw
            if (node->finallyBody) {
                execStatement(node->finallyBody);
            }
            throw;
        }
    } catch (const PanicSignal&) {
        // Panics are NOT caught by try/catch
        if (node->finallyBody) {
            execStatement(node->finallyBody);
        }
        throw;
    } catch (const std::runtime_error& e) {
        // Catch C++ runtime errors as generic Flux errors
        bool caught = false;
        for (auto& clause : node->catchClauses) {
            if (clause.errorType == "error") {
                auto catchEnv = std::make_shared<Environment>(currentEnv);

                auto errObj = std::make_shared<FluxObject>();
                auto errClass = std::make_shared<FluxClass>();
                errClass->name = "error";
                errObj->classDef = errClass;
                errObj->fields["message"] = Value::fromString(std::string(e.what()));
                errObj->fields["code"] = Value::fromInt(0);
                errObj->fields["stack"] = Value::fromString("");

                Value errVal;
                errVal.type = ValueType::OBJECT;
                errVal.objectVal = errObj;
                catchEnv->define(clause.errorName, errVal, clause.errorType);

                execBlock(std::static_pointer_cast<BlockNode>(clause.body), catchEnv);
                caught = true;
                break;
            }
        }
        if (!caught) {
            if (node->finallyBody) {
                execStatement(node->finallyBody);
            }
            throw;
        }
    }

    // Always execute finally
    if (node->finallyBody) {
        execStatement(node->finallyBody);
    }
}

void Interpreter::execImport(std::shared_ptr<ImportStmtNode> node) {
    // Avoid double-importing the same module
    if (importedModules.count(node->path)) return;
    importedModules.insert(node->path);

    if (node->isStdLib) {
        // Route standard library imports to their C++ implementations
        std::string mod = node->path; // e.g. "std.io", "std.net", etc.

        if (mod == "std.io") {
            registerStdIO(globalEnv, *this);
        } else if (mod == "std.net") {
            registerStdNet(globalEnv, *this);
        } else if (mod == "std.collections") {
            registerStdCollections(globalEnv, *this);
        } else if (mod == "std.sys") {
            registerStdSys(globalEnv, *this);
        } else if (mod == "std.json") {
            registerStdJSON(globalEnv, *this);
        } else if (mod == "std.time") {
            registerStdTime(globalEnv, *this);
        } else if (mod == "std.crypto") {
            registerStdCrypto(globalEnv, *this);
        } else if (mod == "std.os") {
            registerStdOS(globalEnv, *this);
        } else if (mod == "std.regex") {
            registerStdRegex(globalEnv, *this);
        } else if (mod == "std.gpu") {
            registerStdGPU(globalEnv, *this);
        } else if (mod == "std.graphics") {
            registerStdGraphics(globalEnv, *this);
        } else if (mod == "std.audio") {
            registerStdAudio(globalEnv, *this);
        } else if (mod == "std.video") {
            registerStdVideo(globalEnv, *this);
        } else if (mod == "std.math") {
            // Math builtins are already registered globally
        } else {
            runtimeError(node->line, "Unknown standard library module: " + mod);
        }
        return;
    }

    // File import: read the file, lex, parse, and execute in the global scope
    std::string path = node->path;
    std::ifstream file(path);
    if (!file.is_open()) {
        runtimeError(node->line, "Cannot open import file: " + path);
        return;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    Lexer lexer(source, path);
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        for (auto& err : lexer.getErrors()) {
            errors.push_back(err);
        }
        return;
    }

    Parser parser(tokens, path);
    auto importedProgram = parser.parse();
    if (parser.hasErrors()) {
        for (auto& err : parser.getErrors()) {
            errors.push_back(err);
        }
        return;
    }

    execute(importedProgram);
}

// ============================================================================
// Expression evaluation
// ============================================================================

Value Interpreter::eval(ASTNodePtr node) {
    if (!node) return Value::nil();

    switch (node->nodeType) {
        case NodeType::LITERAL:
            return evalLiteral(std::static_pointer_cast<LiteralNode>(node));
        case NodeType::STRING_INTERPOLATION: {
            auto interpNode = std::static_pointer_cast<StringInterpolationNode>(node);
            std::string result;
            for (auto& part : interpNode->parts) {
                Value v = eval(part);
                result += v.toString();
            }
            return Value::fromString(result);
        }
        case NodeType::VARIABLE:
            return evalVariable(std::static_pointer_cast<VariableNode>(node));
        case NodeType::BINARY:
            return evalBinary(std::static_pointer_cast<BinaryNode>(node));
        case NodeType::UNARY:
            return evalUnary(std::static_pointer_cast<UnaryNode>(node));
        case NodeType::POSTFIX:
            return evalPostfix(std::static_pointer_cast<PostfixNode>(node));
        case NodeType::ASSIGN:
            return evalAssign(std::static_pointer_cast<AssignNode>(node));
        case NodeType::TYPE_REDEF:
            return evalTypeRedef(std::static_pointer_cast<TypeRedefNode>(node));
        case NodeType::CALL:
            return evalCall(std::static_pointer_cast<CallNode>(node));
        case NodeType::MEMBER_ACCESS:
            return evalMemberAccess(std::static_pointer_cast<MemberAccessNode>(node));
        case NodeType::MEMBER_SET:
            return evalMemberSet(std::static_pointer_cast<MemberSetNode>(node));
        case NodeType::INDEX_ACCESS:
            return evalIndexAccess(std::static_pointer_cast<IndexAccessNode>(node));
        case NodeType::INDEX_SET:
            return evalIndexSet(std::static_pointer_cast<IndexSetNode>(node));
        case NodeType::CAST:
            return evalCast(std::static_pointer_cast<CastNode>(node));
        case NodeType::NEW_EXPR:
            return evalNewExpr(std::static_pointer_cast<NewExprNode>(node));
        case NodeType::LAMBDA:
            return evalLambda(std::static_pointer_cast<LambdaNode>(node));
        case NodeType::LIST_LITERAL:
            return evalListLiteral(std::static_pointer_cast<ListLiteralNode>(node));
        case NodeType::TERNARY:
        case NodeType::MAP_LITERAL:
        case NodeType::STRUCT_INIT:
            // TODO: handle these
            return Value::nil();
        default:
            // If it's a statement node, execute it
            execStatement(node);
            return Value::nil();
    }
}

Value Interpreter::evalLiteral(std::shared_ptr<LiteralNode> node) {
    switch (node->litType) {
        case LiteralNode::INT_LIT:
            return Value::fromInt((int32_t)node->intVal);
        case LiteralNode::LONG_LIT:
            return Value::fromLong(node->intVal);
        case LiteralNode::FLOAT_LIT:
            return Value::fromFloat(node->floatVal);
        case LiteralNode::STRING_LIT:
            return evalStringInterpolation(node->stringVal);
        case LiteralNode::CHAR_LIT:
            return Value::fromChar(node->charVal);
        case LiteralNode::BOOL_LIT:
            return Value::fromBool(node->boolVal);
        case LiteralNode::NULL_LIT:
            return Value::nil();
        case LiteralNode::BYTE_LIT:
            return Value::fromByte((uint8_t)node->intVal);
    }
    return Value::nil();
}

Value Interpreter::evalStringInterpolation(const std::string& str) {
    // Process $ and ${} in the string
    std::string result;
    size_t i = 0;

    while (i < str.size()) {
        if (str[i] == '$' && i + 1 < str.size()) {
            if (str[i + 1] == '{') {
                // ${expression} - find matching }
                size_t start = i + 2;
                int depth = 1;
                size_t end = start;
                while (end < str.size() && depth > 0) {
                    if (str[end] == '{') depth++;
                    else if (str[end] == '}') depth--;
                    if (depth > 0) end++;
                }
                if (depth == 0) {
                    std::string exprStr = str.substr(start, end - start);
                    // Lex and parse the expression
                    Lexer lexer(exprStr + ";", "<interpolation>");
                    auto tokens = lexer.tokenize();
                    if (!lexer.hasErrors() && tokens.size() > 1) {
                        Parser parser(tokens, "<interpolation>");
                        auto exprNode = parser.parse();
                        if (!parser.hasErrors()) {
                            auto prog = std::static_pointer_cast<ProgramNode>(exprNode);
                            if (!prog->declarations.empty()) {
                                auto exprStmt = std::static_pointer_cast<ExpressionStmtNode>(
                                    prog->declarations[0]);
                                Value val = eval(exprStmt->expression);
                                result += val.toString();
                            }
                        }
                    }
                    i = end + 1;
                } else {
                    result += str[i];
                    i++;
                }
            } else if (std::isalpha(str[i + 1]) || str[i + 1] == '_') {
                // $varName - read identifier
                size_t start = i + 1;
                size_t end = start;
                while (end < str.size() && (std::isalnum(str[end]) || str[end] == '_')) {
                    end++;
                }
                std::string varName = str.substr(start, end - start);
                if (currentEnv->has(varName)) {
                    Value val = currentEnv->get(varName);
                    result += val.toString();
                } else {
                    result += "$" + varName;
                }
                i = end;
            } else {
                result += str[i];
                i++;
            }
        } else {
            result += str[i];
            i++;
        }
    }

    return Value::fromString(result);
}

Value Interpreter::evalVariable(std::shared_ptr<VariableNode> node) {
    if (currentEnv->has(node->name)) {
        return currentEnv->get(node->name);
    }
    runtimeError(node->line, "Undefined variable '" + node->name + "'");
    return Value::nil();
}

Value Interpreter::evalBinary(std::shared_ptr<BinaryNode> node) {
    // Short-circuit for logical operators
    if (node->op.type == TokenType::AMP_AMP) {
        Value left = eval(node->left);
        if (!left.isTruthy()) return Value::fromBool(false);
        Value right = eval(node->right);
        return Value::fromBool(right.isTruthy());
    }

    if (node->op.type == TokenType::PIPE_PIPE) {
        Value left = eval(node->left);
        if (left.isTruthy()) return Value::fromBool(true);
        Value right = eval(node->right);
        return Value::fromBool(right.isTruthy());
    }

    // butnot: A && !B
    if (node->op.type == TokenType::KW_BUTNOT) {
        Value left = eval(node->left);
        Value right = eval(node->right);
        return Value::fromBool(left.isTruthy() && !right.isTruthy());
    }

    Value left = eval(node->left);
    Value right = eval(node->right);

    switch (node->op.type) {
        // Equality operators
        case TokenType::EQUAL_EQUAL:
            return Value::fromBool(left.identityEquals(right));
        case TokenType::BANG_EQUAL:
            return Value::fromBool(!left.identityEquals(right));
        case TokenType::EQUAL_NUM_EQUAL:
            return Value::fromBool(left.numericEquals(right));
        case TokenType::EQUAL_WORD_EQUAL:
            return Value::fromBool(left.wordEquals(right));

        // Comparison
        case TokenType::LESS:
            return Value::fromBool(left.toNumber() < right.toNumber());
        case TokenType::LESS_EQUAL:
            return Value::fromBool(left.toNumber() <= right.toNumber());
        case TokenType::GREATER:
            return Value::fromBool(left.toNumber() > right.toNumber());
        case TokenType::GREATER_EQUAL:
            return Value::fromBool(left.toNumber() >= right.toNumber());

        // Arithmetic
        default:
            return doArithmetic(left, node->op, right);
    }
}

Value Interpreter::evalUnary(std::shared_ptr<UnaryNode> node) {
    Value operand = eval(node->operand);

    switch (node->op.type) {
        case TokenType::MINUS:
            if (operand.type == ValueType::INT) return Value::fromInt(-operand.intVal);
            if (operand.type == ValueType::LONG) return Value::fromLong(-operand.longVal);
            if (operand.type == ValueType::FLOAT) return Value::fromFloat(-operand.floatVal);
            return Value::fromFloat(-operand.toNumber());

        case TokenType::BANG:
            return Value::fromBool(!operand.isTruthy());

        case TokenType::PLUS_PLUS: {
            // Pre-increment
            if (node->operand->nodeType == NodeType::VARIABLE) {
                auto varNode = std::static_pointer_cast<VariableNode>(node->operand);
                Value newVal;
                if (operand.type == ValueType::INT) newVal = Value::fromInt(operand.intVal + 1);
                else if (operand.type == ValueType::LONG) newVal = Value::fromLong(operand.longVal + 1);
                else newVal = Value::fromFloat(operand.toNumber() + 1);
                currentEnv->set(varNode->name, newVal);
                return newVal;
            }
            return operand;
        }

        case TokenType::MINUS_MINUS: {
            // Pre-decrement
            if (node->operand->nodeType == NodeType::VARIABLE) {
                auto varNode = std::static_pointer_cast<VariableNode>(node->operand);
                Value newVal;
                if (operand.type == ValueType::INT) newVal = Value::fromInt(operand.intVal - 1);
                else if (operand.type == ValueType::LONG) newVal = Value::fromLong(operand.longVal - 1);
                else newVal = Value::fromFloat(operand.toNumber() - 1);
                currentEnv->set(varNode->name, newVal);
                return newVal;
            }
            return operand;
        }

        default:
            return operand;
    }
}

Value Interpreter::evalPostfix(std::shared_ptr<PostfixNode> node) {
    Value operand = eval(node->operand);
    Value oldVal = operand;

    if (node->operand->nodeType == NodeType::VARIABLE) {
        auto varNode = std::static_pointer_cast<VariableNode>(node->operand);
        Value newVal;

        if (node->op.type == TokenType::PLUS_PLUS) {
            if (operand.type == ValueType::INT) newVal = Value::fromInt(operand.intVal + 1);
            else if (operand.type == ValueType::LONG) newVal = Value::fromLong(operand.longVal + 1);
            else newVal = Value::fromFloat(operand.toNumber() + 1);
        } else if (node->op.type == TokenType::MINUS_MINUS) {
            if (operand.type == ValueType::INT) newVal = Value::fromInt(operand.intVal - 1);
            else if (operand.type == ValueType::LONG) newVal = Value::fromLong(operand.longVal - 1);
            else newVal = Value::fromFloat(operand.toNumber() - 1);
        }

        currentEnv->set(varNode->name, newVal);
    }

    return oldVal; // Postfix returns old value
}

Value Interpreter::evalAssign(std::shared_ptr<AssignNode> node) {
    Value val = eval(node->value);

    if (node->op.type == TokenType::EQUAL) {
        currentEnv->set(node->name, val);
        return val;
    }

    // Compound assignment: +=, -=, *=, /=, %=
    Value current = currentEnv->get(node->name);
    Value result;

    switch (node->op.type) {
        case TokenType::PLUS_EQUAL:
            if (current.type == ValueType::STRING) {
                result = Value::fromString(current.stringVal + val.toString());
            } else {
                result = doArithmetic(current, Token(TokenType::PLUS, "+", 0, 0), val);
            }
            break;
        case TokenType::MINUS_EQUAL:
            result = doArithmetic(current, Token(TokenType::MINUS, "-", 0, 0), val);
            break;
        case TokenType::STAR_EQUAL:
            result = doArithmetic(current, Token(TokenType::STAR, "*", 0, 0), val);
            break;
        case TokenType::SLASH_EQUAL:
            result = doArithmetic(current, Token(TokenType::SLASH, "/", 0, 0), val);
            break;
        case TokenType::PERCENT_EQUAL:
            result = doArithmetic(current, Token(TokenType::PERCENT, "%", 0, 0), val);
            break;
        default:
            result = val;
            break;
    }

    currentEnv->set(node->name, result);
    return result;
}

Value Interpreter::evalTypeRedef(std::shared_ptr<TypeRedefNode> node) {
    Value newVal;
    if (node->value) {
        newVal = eval(node->value);
    } else {
        // Auto-convert existing value: get current value and cast
        if (currentEnv->has(node->name)) {
            Value oldVal = currentEnv->get(node->name);
            newVal = castValue(oldVal, node->newType);
        } else {
            newVal = defaultValueForType(node->newType);
        }
    }

    currentEnv->redefine(node->name, node->newType, newVal);
    return newVal;
}

Value Interpreter::evalCall(std::shared_ptr<CallNode> node) {
    // Special handling for super.method() calls inside constructors
    // Execute in the current environment so field writes propagate correctly
    if (node->callee->nodeType == NodeType::MEMBER_ACCESS) {
        auto memberNode = std::static_pointer_cast<MemberAccessNode>(node->callee);
        if (memberNode->object->nodeType == NodeType::VARIABLE) {
            auto varNode = std::static_pointer_cast<VariableNode>(memberNode->object);
            if (varNode->name == "super" && currentClass && currentClass->parent) {
                auto parentClass = currentClass->parent;
                Value method = parentClass->findMethod(memberNode->member);
                if (method.type == ValueType::FUNCTION && method.functionVal) {
                    // Evaluate arguments
                    std::vector<Value> args;
                    for (auto& arg : node->arguments) {
                        args.push_back(eval(arg));
                    }

                    // Execute the parent method body directly in current env
                    // by binding params into a child env of currentEnv
                    auto superEnv = std::make_shared<Environment>(currentEnv);
                    bindFunctionParams(method.functionVal, args, node->argNames, superEnv);

                    auto prevEnv = currentEnv;
                    auto prevClass = currentClass;
                    currentEnv = superEnv;
                    // CRITICAL: Set currentClass to the parent so that nested
                    // super calls resolve to the grandparent, not back to us.
                    currentClass = parentClass;

                    try {
                        execStatement(method.functionVal->body);
                    } catch (const ReturnSignal&) {
                        // Allow return from super constructor
                    }

                    // Copy any fields modified in superEnv back into the parent env
                    // Use prevClass since we need to check all fields in the
                    // original class hierarchy (not just the parent's fields)
                    if (prevClass) {
                        for (auto& [fieldName, _] : prevClass->fieldDefaults) {
                            if (superEnv->hasLocal(fieldName)) {
                                prevEnv->set(fieldName, superEnv->get(fieldName));
                            }
                        }
                    }
                    // Also copy back parent's own fields
                    if (parentClass) {
                        for (auto& [fieldName, _] : parentClass->fieldDefaults) {
                            if (superEnv->has(fieldName)) {
                                try {
                                    prevEnv->set(fieldName, superEnv->get(fieldName));
                                } catch (...) {
                                    // Field may not exist in prevEnv yet
                                }
                            }
                        }
                    }

                    currentEnv = prevEnv;
                    currentClass = prevClass;
                    return Value::nil();
                }
                runtimeError(node->line, "Parent class '" + parentClass->name +
                             "' has no method '" + memberNode->member + "'");
                return Value::nil();
            }
        }
    }

    // Dynamic resolution of bare method calls within method bodies.
    // When inside a method, if the callee is a bare name not in the current
    // environment, check if it's a method on the current object's class.
    if (node->callee->nodeType == NodeType::VARIABLE && currentObject && currentObject->classDef) {
        auto varNode = std::static_pointer_cast<VariableNode>(node->callee);
        if (!currentEnv->has(varNode->name)) {
            Value method = currentObject->classDef->findMethod(varNode->name);
            if (method.type == ValueType::FUNCTION && method.functionVal) {
                // Bind the method with a fresh snapshot of the object's current fields
                auto boundFunc = std::make_shared<FluxFunction>(*method.functionVal);
                auto methodEnv = std::make_shared<Environment>(boundFunc->closure);
                for (auto& [fieldName, fieldVal] : currentObject->fields) {
                    methodEnv->define(fieldName, fieldVal);
                }
                boundFunc->closure = methodEnv;
                boundFunc->boundObject = currentObject;

                // Evaluate arguments
                std::vector<Value> args;
                for (auto& arg : node->arguments) {
                    args.push_back(eval(arg));
                }

                Value result = callFunction(boundFunc, args, node->argNames);

                // Re-sync: after the inner method writes back to the object,
                // update field copies in the outer method's environment so
                // subsequent reads and the outer method's own write-back
                // reflect the changes made by the inner call.
                for (auto& [fieldName, fieldVal] : currentObject->fields) {
                    if (currentEnv->has(fieldName)) {
                        try {
                            currentEnv->set(fieldName, fieldVal);
                        } catch (...) {}
                    }
                }

                return result;
            }
        }
    }

    Value callee = eval(node->callee);

    // Evaluate arguments
    std::vector<Value> args;
    for (auto& arg : node->arguments) {
        args.push_back(eval(arg));
    }

    if (callee.type == ValueType::NATIVE_FUNCTION) {
        return callNative(callee.nativeFn, args);
    }

    if (callee.type == ValueType::FUNCTION && callee.functionVal) {
        return callFunction(callee.functionVal, args, node->argNames);
    }

    if (callee.type == ValueType::CLASS_DEF && callee.classVal) {
        // Calling a class as a constructor (without new)
        return evalNewExprFromClass(callee.classVal, args, node->argNames);
    }

    // Check if callee name is a type constructor (vec2, vec3, etc.)
    if (node->callee->nodeType == NodeType::VARIABLE) {
        auto varNode = std::static_pointer_cast<VariableNode>(node->callee);
        if (varNode->name == "vec2" || varNode->name == "vec3") {
            // Create a struct-like object
            auto obj = std::make_shared<FluxObject>();
            auto cls = std::make_shared<FluxClass>();
            cls->name = varNode->name;
            obj->classDef = cls;

            if (varNode->name == "vec2" && args.size() >= 2) {
                obj->fields["x"] = Value::fromFloat(args[0].toNumber());
                obj->fields["y"] = Value::fromFloat(args[1].toNumber());
            } else if (varNode->name == "vec3" && args.size() >= 3) {
                obj->fields["x"] = Value::fromFloat(args[0].toNumber());
                obj->fields["y"] = Value::fromFloat(args[1].toNumber());
                obj->fields["z"] = Value::fromFloat(args[2].toNumber());
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = obj;
            return result;
        }
    }

    runtimeError(node->line, "Attempted to call a non-callable value");
    return Value::nil();
}

Value Interpreter::evalNewExprFromClass(std::shared_ptr<FluxClass> classDef,
                                        std::vector<Value> args,
                                        std::vector<std::string> argNames) {
    auto obj = std::make_shared<FluxObject>();
    obj->classDef = classDef;

    // Initialize fields with defaults
    for (auto& [name, defVal] : classDef->fieldDefaults) {
        obj->fields[name] = defVal;
    }

    // Call constructor (init) if it exists
    Value initMethod = classDef->findMethod("init");
    if (initMethod.type == ValueType::FUNCTION && initMethod.functionVal) {
        auto initFunc = initMethod.functionVal;

        // Create a new environment for the constructor
        auto methodEnv = std::make_shared<Environment>(initFunc->closure);

        // Bind 'this' fields - we bind by giving direct access through the object
        // We use a proxy: set each field as a variable
        // For simplicity, bind each field name in the constructor scope
        for (auto& [fieldName, fieldVal] : obj->fields) {
            methodEnv->define(fieldName, fieldVal, classDef->fieldTypes[fieldName]);
        }

        // Bind parameters
        bindFunctionParams(initFunc, args, argNames, methodEnv);

        auto prevEnv = currentEnv;
        auto prevClass = currentClass;
        currentEnv = methodEnv;
        currentClass = classDef;

        try {
            execStatement(initFunc->body);
        } catch (const ReturnSignal&) {
            // Constructor shouldn't return a value, but allow it
        }

        // Copy back any fields that were modified
        for (auto& [fieldName, _] : classDef->fieldDefaults) {
            if (methodEnv->hasLocal(fieldName)) {
                obj->fields[fieldName] = methodEnv->get(fieldName);
            }
        }

        currentEnv = prevEnv;
        currentClass = prevClass;
    }

    Value result;
    result.type = ValueType::OBJECT;
    result.objectVal = obj;
    return result;
}

Value Interpreter::evalMemberAccess(std::shared_ptr<MemberAccessNode> node) {
    // Handle super.method() calls - returns the parent class method
    if (node->object->nodeType == NodeType::VARIABLE) {
        auto varNode = std::static_pointer_cast<VariableNode>(node->object);
        if (varNode->name == "super") {
            // Look up parent class
            if (!currentClass || !currentClass->parent) {
                runtimeError(node->line, "Cannot use 'super' outside of a class with a parent");
                return Value::nil();
            }
            auto parentClass = currentClass->parent;
            Value method = parentClass->findMethod(node->member);
            if (method.type == ValueType::FUNCTION) {
                // Return the parent method bound to the current environment
                // so it can access the current object's fields
                auto boundFunc = std::make_shared<FluxFunction>(*method.functionVal);
                boundFunc->closure = currentEnv;
                Value result;
                result.type = ValueType::FUNCTION;
                result.functionVal = boundFunc;
                return result;
            }
            runtimeError(node->line, "Parent class '" + parentClass->name +
                         "' has no method '" + node->member + "'");
            return Value::nil();
        }
    }

    // Handle type.random and type.static_method patterns
    if (node->object->nodeType == NodeType::VARIABLE) {
        auto varNode = std::static_pointer_cast<VariableNode>(node->object);
        std::string typeName = varNode->name;

        // .random on primitive types
        if (node->member == "random") {
            static std::mt19937 rng(std::chrono::steady_clock::now()
                                        .time_since_epoch().count());

            if (typeName == "int") {
                std::uniform_int_distribution<int32_t> dist;
                return Value::fromInt(dist(rng));
            } else if (typeName == "float") {
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                return Value::fromFloat(dist(rng));
            } else if (typeName == "bool") {
                std::uniform_int_distribution<int> dist(0, 1);
                return Value::fromBool(dist(rng) == 1);
            }
        }
    }

    Value obj = eval(node->object);

    // Object member access
    if (obj.type == ValueType::OBJECT && obj.objectVal) {
        // Check fields first
        auto it = obj.objectVal->fields.find(node->member);
        if (it != obj.objectVal->fields.end()) {
            return it->second;
        }

        // Check methods
        if (obj.objectVal->classDef) {
            Value method = obj.objectVal->classDef->findMethod(node->member);
            if (method.type == ValueType::FUNCTION) {
                // Bind 'this' to the method
                auto boundFunc = std::make_shared<FluxFunction>(*method.functionVal);
                // Store the object as a closure variable
                auto methodEnv = std::make_shared<Environment>(boundFunc->closure);
                // Bind all object fields into the method scope
                for (auto& [fieldName, fieldVal] : obj.objectVal->fields) {
                    methodEnv->define(fieldName, fieldVal);
                }
                boundFunc->closure = methodEnv;

                // Store a ref to the object for field write-back
                boundFunc->boundObject = obj.objectVal;

                Value result;
                result.type = ValueType::FUNCTION;
                result.functionVal = boundFunc;
                return result;
            }
        }

        return Value::nil();
    }

    // Enum member access
    if (obj.type == ValueType::ENUM_DEF && obj.enumVal) {
        auto it = obj.enumVal->values.find(node->member);
        if (it != obj.enumVal->values.end()) {
            Value ev;
            ev.type = ValueType::ENUM_VALUE;
            ev.enumName = obj.enumVal->name;
            ev.enumMember = node->member;
            ev.enumIntVal = it->second;
            return ev;
        }
        runtimeError(node->line, "Enum '" + obj.enumVal->name + "' has no member '" +
                     node->member + "'");
    }

    // String methods
    if (obj.type == ValueType::STRING) {
        if (node->member == "length") {
            return Value::fromInt((int)obj.stringVal.size());
        }
        // substring(start, length) -> returns portion of string
        if (node->member == "substring") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromString(s);
                int start = (int)args[0].toNumber();
                if (start < 0) start = 0;
                if (start >= (int)s.size()) return Value::fromString("");
                if (args.size() >= 2) {
                    int len = (int)args[1].toNumber();
                    return Value::fromString(s.substr(start, len));
                }
                return Value::fromString(s.substr(start));
            };
            return fn;
        }
        // indexOf(substr) -> returns index or -1
        if (node->member == "indexOf") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromInt(-1);
                auto pos = s.find(args[0].toString());
                return Value::fromInt(pos == std::string::npos ? -1 : (int)pos);
            };
            return fn;
        }
        // contains(substr) -> bool
        if (node->member == "contains") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromBool(false);
                return Value::fromBool(s.find(args[0].toString()) != std::string::npos);
            };
            return fn;
        }
        // startsWith(prefix) -> bool
        if (node->member == "startsWith") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromBool(false);
                std::string prefix = args[0].toString();
                return Value::fromBool(s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0);
            };
            return fn;
        }
        // endsWith(suffix) -> bool
        if (node->member == "endsWith") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromBool(false);
                std::string suffix = args[0].toString();
                return Value::fromBool(s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
            };
            return fn;
        }
        // split(delimiter) -> list of strings
        if (node->member == "split") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                std::string delim = args.empty() ? " " : args[0].toString();
                auto list = std::make_shared<std::vector<Value>>();
                if (delim.empty()) {
                    for (char c : s) list->push_back(Value::fromString(std::string(1, c)));
                } else {
                    size_t start = 0, pos;
                    while ((pos = s.find(delim, start)) != std::string::npos) {
                        list->push_back(Value::fromString(s.substr(start, pos - start)));
                        start = pos + delim.size();
                    }
                    list->push_back(Value::fromString(s.substr(start)));
                }
                Value result;
                result.type = ValueType::LIST;
                result.listVal = list;
                return result;
            };
            return fn;
        }
        // trim() -> removes leading/trailing whitespace
        if (node->member == "trim") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                std::string result = s;
                size_t start = result.find_first_not_of(" \t\n\r");
                size_t end = result.find_last_not_of(" \t\n\r");
                if (start == std::string::npos) return Value::fromString("");
                return Value::fromString(result.substr(start, end - start + 1));
            };
            return fn;
        }
        // toUpper() -> uppercase string
        if (node->member == "toUpper") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                std::string r = s;
                for (auto& c : r) c = toupper(c);
                return Value::fromString(r);
            };
            return fn;
        }
        // toLower() -> lowercase string
        if (node->member == "toLower") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                std::string r = s;
                for (auto& c : r) c = tolower(c);
                return Value::fromString(r);
            };
            return fn;
        }
        // replace(old, new) -> replace all occurrences
        if (node->member == "replace") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.size() < 2) return Value::fromString(s);
                std::string oldStr = args[0].toString();
                std::string newStr = args[1].toString();
                std::string result = s;
                size_t pos = 0;
                while ((pos = result.find(oldStr, pos)) != std::string::npos) {
                    result.replace(pos, oldStr.length(), newStr);
                    pos += newStr.length();
                }
                return Value::fromString(result);
            };
            return fn;
        }
        // charAt(index) -> single character string
        if (node->member == "charAt") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromString("");
                int idx = (int)args[0].toNumber();
                if (idx < 0 || idx >= (int)s.size()) return Value::fromString("");
                return Value::fromString(std::string(1, s[idx]));
            };
            return fn;
        }
        // reverse() -> reversed string
        if (node->member == "reverse") {
            std::string s = obj.stringVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [s](Interpreter& interp, std::vector<Value> args) -> Value {
                std::string r = s;
                std::reverse(r.begin(), r.end());
                return Value::fromString(r);
            };
            return fn;
        }
    }

    // List methods/properties
    if (obj.type == ValueType::LIST && obj.listVal) {
        if (node->member == "length") {
            return Value::fromInt((int)obj.listVal->size());
        }
        if (node->member == "add") {
            auto listRef = obj.listVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [listRef](Interpreter& interp, std::vector<Value> args) -> Value {
                if (!args.empty()) listRef->push_back(args[0]);
                return Value::nil();
            };
            return fn;
        }
        if (node->member == "removeAt") {
            auto listRef = obj.listVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [listRef](Interpreter& interp, std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    int idx = args[0].intVal;
                    if (idx >= 0 && idx < (int)listRef->size()) {
                        listRef->erase(listRef->begin() + idx);
                    }
                }
                return Value::nil();
            };
            return fn;
        }
        if (node->member == "contains") {
            auto listRef = obj.listVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [listRef](Interpreter& interp, std::vector<Value> args) -> Value {
                if (args.empty()) return Value::fromBool(false);
                for (auto& item : *listRef) {
                    if (item.identityEquals(args[0])) return Value::fromBool(true);
                }
                return Value::fromBool(false);
            };
            return fn;
        }
        if (node->member == "clear") {
            auto listRef = obj.listVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [listRef](Interpreter& interp, std::vector<Value> args) -> Value {
                listRef->clear();
                return Value::nil();
            };
            return fn;
        }
        if (node->member == "sort") {
            auto listRef = obj.listVal;
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [listRef](Interpreter& interp, std::vector<Value> args) -> Value {
                if (!args.empty() && args[0].type == ValueType::FUNCTION) {
                    auto cmpFunc = args[0].functionVal;
                    std::sort(listRef->begin(), listRef->end(),
                              [&](const Value& a, const Value& b) -> bool {
                                  Value result = interp.callFunction(cmpFunc, {a, b});
                                  return result.isTruthy();
                              });
                } else {
                    std::sort(listRef->begin(), listRef->end(),
                              [](const Value& a, const Value& b) -> bool {
                                  return a.toNumber() < b.toNumber();
                              });
                }
                return Value::nil();
            };
            return fn;
        }
    }

    return Value::nil();
}

Value Interpreter::evalMemberSet(std::shared_ptr<MemberSetNode> node) {
    Value obj = eval(node->object);
    Value val = eval(node->value);

    if (obj.type == ValueType::OBJECT && obj.objectVal) {
        obj.objectVal->setField(node->member, val);
        return val;
    }

    runtimeError(node->line, "Cannot set member '" + node->member + "' on non-object");
    return Value::nil();
}

Value Interpreter::evalIndexAccess(std::shared_ptr<IndexAccessNode> node) {
    Value obj = eval(node->object);
    Value index = eval(node->index);

    if (obj.type == ValueType::LIST && obj.listVal) {
        int idx = (int)index.toNumber();
        if (idx < 0 || idx >= (int)obj.listVal->size()) {
            throw FluxException("IndexError",
                                "List index " + std::to_string(idx) + " out of bounds (size: " +
                                std::to_string(obj.listVal->size()) + ")");
        }
        return (*obj.listVal)[idx];
    }

    if (obj.type == ValueType::STRING) {
        int idx = (int)index.toNumber();
        if (idx < 0 || idx >= (int)obj.stringVal.size()) {
            throw FluxException("IndexError",
                                "String index " + std::to_string(idx) + " out of bounds");
        }
        return Value::fromString(std::string(1, obj.stringVal[idx]));
    }

    runtimeError(node->line, "Cannot index into value of type '" + obj.typeName() + "'");
    return Value::nil();
}

Value Interpreter::evalIndexSet(std::shared_ptr<IndexSetNode> node) {
    Value obj = eval(node->object);
    Value index = eval(node->index);
    Value val = eval(node->value);

    if (obj.type == ValueType::LIST && obj.listVal) {
        int idx = (int)index.toNumber();
        if (idx < 0 || idx >= (int)obj.listVal->size()) {
            throw FluxException("IndexError",
                                "List index " + std::to_string(idx) + " out of bounds");
        }
        (*obj.listVal)[idx] = val;
        return val;
    }

    runtimeError(node->line, "Cannot index-assign into value of type '" +
                 obj.typeName() + "'");
    return Value::nil();
}

Value Interpreter::evalCast(std::shared_ptr<CastNode> node) {
    Value val = eval(node->expr);
    return castValue(val, node->targetType);
}

Value Interpreter::evalNewExpr(std::shared_ptr<NewExprNode> node) {
    // Look up the class
    if (!currentEnv->has(node->className)) {
        runtimeError(node->line, "Undefined class '" + node->className + "'");
        return Value::nil();
    }

    Value classVal = currentEnv->get(node->className);
    if (classVal.type != ValueType::CLASS_DEF || !classVal.classVal) {
        runtimeError(node->line, "'" + node->className + "' is not a class");
        return Value::nil();
    }

    std::vector<Value> args;
    for (auto& arg : node->arguments) {
        args.push_back(eval(arg));
    }

    return evalNewExprFromClass(classVal.classVal, args, node->argNames);
}

Value Interpreter::evalLambda(std::shared_ptr<LambdaNode> node) {
    auto func = std::make_shared<FluxFunction>();
    func->name = "<lambda>";
    func->params = node->params;
    func->returnType = node->returnType;
    func->body = node->body;
    func->closure = currentEnv;
    func->isMethod = false;

    Value funcVal;
    funcVal.type = ValueType::FUNCTION;
    funcVal.functionVal = func;
    return funcVal;
}

Value Interpreter::evalListLiteral(std::shared_ptr<ListLiteralNode> node) {
    auto list = std::make_shared<std::vector<Value>>();
    for (auto& elem : node->elements) {
        list->push_back(eval(elem));
    }
    return Value::fromList(list);
}

// ============================================================================
// Function invocation
// ============================================================================

void Interpreter::bindFunctionParams(std::shared_ptr<FluxFunction> func,
                                     std::vector<Value>& args,
                                     std::vector<std::string>& argNames,
                                     std::shared_ptr<Environment> env) {
    auto& params = func->params;

    // Build a map of named arguments
    std::unordered_map<std::string, Value> namedArgs;
    std::vector<Value> positionalArgs;

    for (size_t i = 0; i < args.size(); i++) {
        if (i < argNames.size() && !argNames[i].empty()) {
            namedArgs[argNames[i]] = args[i];
        } else {
            positionalArgs.push_back(args[i]);
        }
    }

    // Bind parameters
    size_t posIdx = 0;
    for (size_t i = 0; i < params.size(); i++) {
        auto& param = params[i];
        auto namedIt = namedArgs.find(param.name);

        if (namedIt != namedArgs.end()) {
            env->define(param.name, namedIt->second, param.typeName);
        } else if (posIdx < positionalArgs.size()) {
            env->define(param.name, positionalArgs[posIdx++], param.typeName);
        } else if (param.defaultValue) {
            env->define(param.name, eval(param.defaultValue), param.typeName);
        } else {
            env->define(param.name, defaultValueForType(param.typeName), param.typeName);
        }
    }
}

Value Interpreter::invokeFunction(std::shared_ptr<FluxFunction> func, std::vector<Value> args) {
    return callFunction(func, args);
}

Value Interpreter::callFunction(std::shared_ptr<FluxFunction> func,
                                std::vector<Value> args,
                                std::vector<std::string> argNames) {
    auto funcEnv = std::make_shared<Environment>(func->closure);

    bindFunctionParams(func, args, argNames, funcEnv);

    auto prevEnv = currentEnv;
    currentEnv = funcEnv;

    // Track current object for bare method call resolution within methods
    auto prevObject = currentObject;
    if (func->isMethod && func->boundObject) {
        currentObject = func->boundObject;
    }

    Value result;
    try {
        execStatement(func->body);

        // Default return value based on return type
        if (func->returnType.empty() || func->returnType == "int") {
            result = Value::fromInt(0);
        } else {
            result = Value::nil();
        }
    } catch (const ReturnSignal& ret) {
        result = ret.value;
    } catch (...) {
        // Write back object fields if this is a method
        if (func->isMethod && func->boundObject) {
            for (auto& [fieldName, _] : func->boundObject->fields) {
                if (funcEnv->has(fieldName)) {
                    func->boundObject->fields[fieldName] = funcEnv->get(fieldName);
                }
            }
        }
        currentEnv = prevEnv;
        currentObject = prevObject;
        throw;
    }

    // Write back object fields if this is a method
    if (func->isMethod && func->boundObject) {
        for (auto& [fieldName, _] : func->boundObject->fields) {
            // Check the entire scope chain (funcEnv + closure) for field values
            if (funcEnv->has(fieldName)) {
                func->boundObject->fields[fieldName] = funcEnv->get(fieldName);
            }
        }
    }

    currentEnv = prevEnv;
    currentObject = prevObject;
    return result;
}

Value Interpreter::callNative(Value::NativeFn fn, std::vector<Value> args) {
    return fn(*this, args);
}

// ============================================================================
// Arithmetic operations
// ============================================================================

Value Interpreter::doArithmetic(const Value& left, const Token& op, const Value& right) {
    // String concatenation with +
    if (op.type == TokenType::PLUS) {
        if (left.type == ValueType::STRING || right.type == ValueType::STRING) {
            return Value::fromString(left.toString() + right.toString());
        }
    }

    // If both are int, keep int
    if (left.type == ValueType::INT && right.type == ValueType::INT) {
        switch (op.type) {
            case TokenType::PLUS:    return Value::fromInt(left.intVal + right.intVal);
            case TokenType::MINUS:   return Value::fromInt(left.intVal - right.intVal);
            case TokenType::STAR:    return Value::fromInt(left.intVal * right.intVal);
            case TokenType::SLASH:
                if (right.intVal == 0) {
                    throw FluxException("error", "Division by zero");
                }
                return Value::fromInt(left.intVal / right.intVal);
            case TokenType::PERCENT:
                if (right.intVal == 0) {
                    throw FluxException("error", "Modulo by zero");
                }
                return Value::fromInt(left.intVal % right.intVal);
            default: break;
        }
    }

    // Both long
    if (left.type == ValueType::LONG && right.type == ValueType::LONG) {
        switch (op.type) {
            case TokenType::PLUS:    return Value::fromLong(left.longVal + right.longVal);
            case TokenType::MINUS:   return Value::fromLong(left.longVal - right.longVal);
            case TokenType::STAR:    return Value::fromLong(left.longVal * right.longVal);
            case TokenType::SLASH:
                if (right.longVal == 0) throw FluxException("error", "Division by zero");
                return Value::fromLong(left.longVal / right.longVal);
            case TokenType::PERCENT:
                if (right.longVal == 0) throw FluxException("error", "Modulo by zero");
                return Value::fromLong(left.longVal % right.longVal);
            default: break;
        }
    }

    // Mixed numeric: promote to float
    double l = left.toNumber();
    double r = right.toNumber();

    switch (op.type) {
        case TokenType::PLUS:    return Value::fromFloat(l + r);
        case TokenType::MINUS:   return Value::fromFloat(l - r);
        case TokenType::STAR:    return Value::fromFloat(l * r);
        case TokenType::SLASH:
            // Float division by zero returns inf per IEEE 754
            return Value::fromFloat(l / r);
        case TokenType::PERCENT:
            if (r == 0.0) throw FluxException("error", "Modulo by zero");
            return Value::fromFloat(std::fmod(l, r));
        default:
            return Value::nil();
    }
}

// ============================================================================
// Type helpers
// ============================================================================

Value Interpreter::defaultValueForType(const std::string& typeName) {
    if (typeName == "void")   return Value::nil();
    if (typeName == "bool")   return Value::fromBool(false);
    if (typeName == "char")   return Value::fromChar('\0');
    if (typeName == "byte")   return Value::fromByte(0);
    if (typeName == "int")    return Value::fromInt(0);
    if (typeName == "long")   return Value::fromLong(0);
    if (typeName == "float")  return Value::fromFloat(0.0);
    if (typeName == "string") return Value::fromString("");
    // For generic/unknown types, default to nil
    return Value::nil();
}

Value Interpreter::castValue(const Value& val, const std::string& targetType) {
    if (targetType == "int") {
        if (val.type == ValueType::INT) return val;
        if (val.type == ValueType::FLOAT) return Value::fromInt((int32_t)val.floatVal);
        if (val.type == ValueType::LONG) return Value::fromInt((int32_t)val.longVal);
        if (val.type == ValueType::BYTE) return Value::fromInt((int32_t)val.byteVal);
        if (val.type == ValueType::CHAR) return Value::fromInt((int32_t)val.charVal);
        if (val.type == ValueType::BOOL) return Value::fromInt(val.boolVal ? 1 : 0);
        if (val.type == ValueType::STRING) {
            try {
                return Value::fromInt((int32_t)std::stoi(val.stringVal));
            } catch (...) {
                throw PanicSignal("Cannot cast string '" + val.stringVal + "' to int");
            }
        }
        return Value::fromInt(0);
    }

    if (targetType == "long") {
        if (val.type == ValueType::LONG) return val;
        if (val.type == ValueType::INT) return Value::fromLong((int64_t)val.intVal);
        if (val.type == ValueType::FLOAT) return Value::fromLong((int64_t)val.floatVal);
        if (val.type == ValueType::STRING) {
            try {
                return Value::fromLong(std::stoll(val.stringVal));
            } catch (...) {
                throw PanicSignal("Cannot cast string '" + val.stringVal + "' to long");
            }
        }
        return Value::fromLong((int64_t)val.toNumber());
    }

    if (targetType == "float") {
        if (val.type == ValueType::FLOAT) return val;
        return Value::fromFloat(val.toNumber());
    }

    if (targetType == "string") {
        return Value::fromString(val.toString());
    }

    if (targetType == "bool") {
        return Value::fromBool(val.isTruthy());
    }

    if (targetType == "char") {
        if (val.type == ValueType::CHAR) return val;
        if (val.type == ValueType::INT) return Value::fromChar((char)val.intVal);
        if (val.type == ValueType::STRING && !val.stringVal.empty())
            return Value::fromChar(val.stringVal[0]);
        return Value::fromChar('\0');
    }

    if (targetType == "byte") {
        if (val.type == ValueType::BYTE) return val;
        return Value::fromByte((uint8_t)val.toNumber());
    }

    // If can't convert, return null
    return Value::nil();
}

std::string Interpreter::fluxTypeName(const Value& val) {
    return val.typeName();
}

bool Interpreter::typeMatches(const Value& val, const std::string& typeName) {
    return val.typeName() == typeName;
}

// ============================================================================
// Error helpers
// ============================================================================

void Interpreter::runtimeError(int line, const std::string& message) {
    std::string err = "Runtime error";
    if (line > 0) err += " [line " + std::to_string(line) + "]";
    err += ": " + message;
    errors.push_back(err);
    throw std::runtime_error(err);
}
