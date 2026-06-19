#pragma once

#include "ast.h"
#include <string>
#include <sstream>
#include <set>
#include <vector>

// ============================================================================
// Flux AOT Compiler — Transpiles Flux AST to C++ source code
//
// This is the first stage toward self-hosting. The transpiler walks the
// Flux AST and emits equivalent C++ code that can be compiled with g++.
//
// Compilation modes:
//   flux compile <file.flux>                 Default (-O2)
//   flux compile <file.flux> -o <output>     Specify output binary name
//   flux compile <file.flux> --fast           -O0 (fast compile)
//   flux compile <file.flux> --release        -O3 (max optimization)
//   flux compile <file.flux> --size           -Os (min binary size)
// ============================================================================

class Transpiler {
public:
    Transpiler();

    // Transpile a parsed Flux program to C++ source code
    std::string transpile(ASTNodePtr program);

    // Full compile pipeline: Flux source -> C++ -> g++ -> binary
    bool compile(const std::string& fluxSource,
                 const std::string& outputPath,
                 const std::string& optimizationLevel = "-O2",
                 bool devMode = false);

    // Base directory for resolving relative imports
    std::string baseDir;

private:
    std::stringstream header;
    std::stringstream forward;
    std::stringstream body;
    std::stringstream functions;
    int indentLevel;
    int tempVarCounter;
    std::set<std::string> usedHeaders;
    std::set<std::string> importedModules;  // Track imported std modules
    bool inTopLevel;  // True when emitting to body (main), false when in function

    // Indentation helpers
    std::string indent();
    void pushIndent();
    void popIndent();

    // Code generation
    void emitNode(ASTNodePtr node, std::stringstream& out);
    void emitProgram(std::shared_ptr<ProgramNode> node, std::stringstream& out);
    void emitVarDecl(std::shared_ptr<VarDeclNode> node, std::stringstream& out);
    void emitFuncDecl(std::shared_ptr<FuncDeclNode> node);
    void emitClassDecl(std::shared_ptr<ClassDeclNode> node);
    void emitEnumDecl(std::shared_ptr<EnumDeclNode> node);
    void emitBlock(std::shared_ptr<BlockNode> node, std::stringstream& out);
    void emitIf(std::shared_ptr<IfStmtNode> node, std::stringstream& out);
    void emitSwitch(std::shared_ptr<SwitchStmtNode> node, std::stringstream& out);
    void emitWhile(std::shared_ptr<WhileStmtNode> node, std::stringstream& out);
    void emitDoWhile(std::shared_ptr<DoWhileStmtNode> node, std::stringstream& out);
    void emitFor(std::shared_ptr<ForStmtNode> node, std::stringstream& out);
    void emitForEach(std::shared_ptr<ForEachStmtNode> node, std::stringstream& out);
    void emitReturn(std::shared_ptr<ReturnStmtNode> node, std::stringstream& out);
    void emitExprStmt(std::shared_ptr<ExpressionStmtNode> node, std::stringstream& out);
    void emitTryCatch(std::shared_ptr<TryCatchNode> node, std::stringstream& out);
    void emitThrow(std::shared_ptr<ThrowStmtNode> node, std::stringstream& out);

    // Expression code generation -> returns C++ expression string
    std::string emitExpr(ASTNodePtr node);
    std::string emitBinaryOp(std::shared_ptr<BinaryNode> node);
    std::string emitUnaryOp(std::shared_ptr<UnaryNode> node);
    std::string emitPostfix(std::shared_ptr<PostfixNode> node);
    std::string emitCall(std::shared_ptr<CallNode> node);
    std::string emitMemberAccess(std::shared_ptr<MemberAccessNode> node);
    std::string emitMemberSet(std::shared_ptr<MemberSetNode> node);
    std::string emitIndexAccess(std::shared_ptr<IndexAccessNode> node);
    std::string emitIndexSet(std::shared_ptr<IndexSetNode> node);
    std::string emitLiteral(std::shared_ptr<LiteralNode> node);
    std::string emitVariable(std::shared_ptr<VariableNode> node);
    std::string emitAssignment(std::shared_ptr<AssignNode> node);
    std::string emitCast(std::shared_ptr<CastNode> node);
    std::string emitNewExpr(std::shared_ptr<NewExprNode> node);
    std::string emitLambda(std::shared_ptr<LambdaNode> node);
    std::string emitListLiteral(std::shared_ptr<ListLiteralNode> node);
    std::string emitStringInterpolation(std::shared_ptr<StringInterpolationNode> node);
    // Handle raw string interpolation ($var and ${expr}) from STRING_LIT
    std::string emitStringInterpFromRaw(const std::string& rawStr);

    // Type mapping
    std::string fluxTypeToC(const std::string& fluxType);

    // Track class names so we know when a type is a user class
    std::set<std::string> classNames;
    // Track enum names
    std::set<std::string> enumNames;
    // Track parent class for super.method() resolution
    std::string currentParentClass;
    // Track which variables are list/vector types
    std::set<std::string> listVars;
    // Track which variables are FluxObject (dynamic object) types
    std::set<std::string> objectVars;
    // Track which variables are pointer types (for -> member access)
    std::set<std::string> pointerVars;
    // Stack to save/restore pointerVars on function entry/exit
    std::vector<std::set<std::string>> pointerVarsStack;
    // Whether FluxObject struct needs to be emitted (only when actually used)
    bool needsFluxObject = false;
    // Freestanding AOT mode (for OS kernel compilation)
    bool freestandingMode = false;
    // Track already-imported files to avoid duplicates
    std::set<std::string> importedFiles;

    // Unique temporary variable
    std::string tempVar();

    // Add a header include
    void requireHeader(const std::string& header);

    // Import resolution: parse and merge declarations from an imported file
    void resolveImport(const std::string& importPath, std::stringstream& out);

    // Check if source has // DOCTYPE {AOT} directive
    static bool hasDocTypeAOT(const std::string& source);
};
