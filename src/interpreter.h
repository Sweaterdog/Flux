#pragma once

#include "ast.h"
#include "value.h"
#include "environment.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <set>

// ============================================================================
// Flux Interpreter - Tree-walking interpreter
// ============================================================================

class Interpreter {
public:
    Interpreter();

    // Execute a parsed program
    void execute(ASTNodePtr program);

    // Execute a single node and return its value
    Value eval(ASTNodePtr node);

    // Access to the global environment (for REPL, etc.)
    std::shared_ptr<Environment> getGlobalEnv() const { return globalEnv; }

    const std::vector<std::string>& getErrors() const { return errors; }
    bool hasErrors() const { return !errors.empty(); }

    // Public interface for calling a Flux function (used by std library modules)
    Value invokeFunction(std::shared_ptr<FluxFunction> func, std::vector<Value> args);

private:
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> currentEnv;
    std::vector<std::string> errors;

    // Whether we're inside an unsafe block
    bool inUnsafe = false;

    // Current class being constructed (for super.init() calls)
    std::shared_ptr<FluxClass> currentClass = nullptr;

    // Current object whose method is executing (for bare method call resolution)
    std::shared_ptr<FluxObject> currentObject = nullptr;

    // Track which modules have already been imported (avoid double-import)
    std::set<std::string> importedModules;

    // ========================================================================
    // Registration of built-in functions and types
    // ========================================================================
    void registerBuiltins();
    void registerMathBuiltins();

    // ========================================================================
    // Statement execution
    // ========================================================================
    void execStatement(ASTNodePtr node);
    void execVarDecl(std::shared_ptr<VarDeclNode> node);
    void execBlock(std::shared_ptr<BlockNode> node, std::shared_ptr<Environment> env);
    void execIf(std::shared_ptr<IfStmtNode> node);
    void execSwitch(std::shared_ptr<SwitchStmtNode> node);
    void execFor(std::shared_ptr<ForStmtNode> node);
    void execForEach(std::shared_ptr<ForEachStmtNode> node);
    void execWhile(std::shared_ptr<WhileStmtNode> node);
    void execDoWhile(std::shared_ptr<DoWhileStmtNode> node);
    void execFuncDecl(std::shared_ptr<FuncDeclNode> node);
    void execClassDecl(std::shared_ptr<ClassDeclNode> node);
    void execStructDecl(std::shared_ptr<StructDeclNode> node);
    void execEnumDecl(std::shared_ptr<EnumDeclNode> node);
    void execTryCatch(std::shared_ptr<TryCatchNode> node);
    void execImport(std::shared_ptr<ImportStmtNode> node);

    // ========================================================================
    // Expression evaluation
    // ========================================================================
    Value evalLiteral(std::shared_ptr<LiteralNode> node);
    Value evalStringInterpolation(const std::string& str);
    Value evalVariable(std::shared_ptr<VariableNode> node);
    Value evalBinary(std::shared_ptr<BinaryNode> node);
    Value evalUnary(std::shared_ptr<UnaryNode> node);
    Value evalPostfix(std::shared_ptr<PostfixNode> node);
    Value evalAssign(std::shared_ptr<AssignNode> node);
    Value evalTypeRedef(std::shared_ptr<TypeRedefNode> node);
    Value evalCall(std::shared_ptr<CallNode> node);
    Value evalMemberAccess(std::shared_ptr<MemberAccessNode> node);
    Value evalMemberSet(std::shared_ptr<MemberSetNode> node);
    Value evalIndexAccess(std::shared_ptr<IndexAccessNode> node);
    Value evalIndexSet(std::shared_ptr<IndexSetNode> node);
    Value evalCast(std::shared_ptr<CastNode> node);
    Value evalNewExpr(std::shared_ptr<NewExprNode> node);
    Value evalLambda(std::shared_ptr<LambdaNode> node);
    Value evalListLiteral(std::shared_ptr<ListLiteralNode> node);

    // ========================================================================
    // Function/method invocation
    // ========================================================================
    Value callFunction(std::shared_ptr<FluxFunction> func, std::vector<Value> args,
                       std::vector<std::string> argNames = {});
    Value callNative(Value::NativeFn fn, std::vector<Value> args);
    Value evalNewExprFromClass(std::shared_ptr<FluxClass> classDef,
                               std::vector<Value> args,
                               std::vector<std::string> argNames);
    void bindFunctionParams(std::shared_ptr<FluxFunction> func,
                            std::vector<Value>& args,
                            std::vector<std::string>& argNames,
                            std::shared_ptr<Environment> env);

    // ========================================================================
    // Type helpers
    // ========================================================================
    Value defaultValueForType(const std::string& typeName);
    Value castValue(const Value& val, const std::string& targetType);
    std::string fluxTypeName(const Value& val);
    bool typeMatches(const Value& val, const std::string& typeName);

    // ========================================================================
    // Math helpers
    // ========================================================================
    Value doArithmetic(const Value& left, const Token& op, const Value& right);

    // ========================================================================
    // Error helpers
    // ========================================================================
    void runtimeError(int line, const std::string& message);
};
