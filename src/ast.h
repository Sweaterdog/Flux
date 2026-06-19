#pragma once

#include "token.h"
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// AST Node Types for the Flux Programming Language
//
// All AST nodes derive from ASTNode. We use shared_ptr for tree ownership.
// ============================================================================

// Forward declarations
struct ASTNode;
using ASTNodePtr = std::shared_ptr<ASTNode>;

// Parameter definition (for function declarations)
struct Parameter {
    std::string typeName;    // e.g. "int", "string", "Player"
    std::string name;
    ASTNodePtr defaultValue; // nullptr if no default
    bool isGeneric = false;
};

// ============================================================================
// Node type enumeration
// ============================================================================

enum class NodeType {
    // Expressions
    LITERAL,
    STRING_INTERPOLATION,
    VARIABLE,
    BINARY,
    UNARY,
    POSTFIX,
    ASSIGN,
    TYPE_REDEF,
    CALL,
    MEMBER_ACCESS,
    MEMBER_SET,
    INDEX_ACCESS,
    INDEX_SET,
    CAST,
    NEW_EXPR,
    LAMBDA,
    LIST_LITERAL,
    MAP_LITERAL,
    STRUCT_INIT,
    TERNARY,
    DEREF_ASSIGN,

    // Statements
    EXPRESSION_STMT,
    VAR_DECL,
    BLOCK,
    IF_STMT,
    SWITCH_STMT,
    FOR_STMT,
    FOR_EACH_STMT,
    WHILE_STMT,
    DO_WHILE_STMT,
    BREAK_STMT,
    CONTINUE_STMT,
    RETURN_STMT,
    FUNC_DECL,
    CLASS_DECL,
    STRUCT_DECL,
    ENUM_DECL,
    INTERFACE_DECL,
    IMPORT_STMT,
    EXPORT_STMT,
    TRY_CATCH,
    THROW_STMT,
    PANIC_STMT,
    UNSAFE_BLOCK,
    ASM_STMT,
    CLEANUP_STMT,
    PROGRAM,
};

// ============================================================================
// Base AST Node
// ============================================================================

struct ASTNode {
    NodeType nodeType;
    int line = 0;
    int column = 0;

    ASTNode(NodeType type) : nodeType(type) {}
    virtual ~ASTNode() = default;
};

// ============================================================================
// Expressions
// ============================================================================

// Literal value: 42, 3.14, "hello", 'A', true, false, null
struct LiteralNode : ASTNode {
    enum LitType { INT_LIT, LONG_LIT, FLOAT_LIT, STRING_LIT, CHAR_LIT, BOOL_LIT, NULL_LIT, BYTE_LIT };
    LitType litType;
    int64_t intVal = 0;
    double floatVal = 0.0;
    std::string stringVal;
    char charVal = '\0';
    bool boolVal = false;

    LiteralNode() : ASTNode(NodeType::LITERAL) {}
};

// String interpolation: "Hello, $name! You have ${score * 2} points."
struct StringInterpolationNode : ASTNode {
    // Parts alternate between literal strings and expressions
    // Literal parts stored as LiteralNode(STRING_LIT), expression parts as any expr
    std::vector<ASTNodePtr> parts;

    StringInterpolationNode() : ASTNode(NodeType::STRING_INTERPOLATION) {}
};

// Variable reference: myVar
struct VariableNode : ASTNode {
    std::string name;

    VariableNode(const std::string& n) : ASTNode(NodeType::VARIABLE), name(n) {}
};

// Binary expression: a + b, x == y, a butnot b
struct BinaryNode : ASTNode {
    ASTNodePtr left;
    Token op;
    ASTNodePtr right;

    BinaryNode() : ASTNode(NodeType::BINARY) {}
};

// Unary expression: !x, -x, ++x, --x
struct UnaryNode : ASTNode {
    Token op;
    ASTNodePtr operand;
    bool isPrefix = true;

    UnaryNode() : ASTNode(NodeType::UNARY) {}
};

// Postfix expression: x++, x--
struct PostfixNode : ASTNode {
    ASTNodePtr operand;
    Token op;

    PostfixNode() : ASTNode(NodeType::POSTFIX) {}
};

// Assignment: x = 5, x += 3
struct AssignNode : ASTNode {
    std::string name;
    Token op;       // =, +=, -=, *=, /=, %=
    ASTNodePtr value;

    AssignNode() : ASTNode(NodeType::ASSIGN) {}
};

// Type re-definition: x = string = "hello"
struct TypeRedefNode : ASTNode {
    std::string name;
    std::string newType;
    ASTNodePtr value;    // can be nullptr (e.g. x = string;)

    TypeRedefNode() : ASTNode(NodeType::TYPE_REDEF) {}
};

// Function/method call: foo(1, 2), obj.bar(x)
struct CallNode : ASTNode {
    ASTNodePtr callee;
    std::vector<ASTNodePtr> arguments;
    std::vector<std::string> argNames;  // for named arguments

    CallNode() : ASTNode(NodeType::CALL) {}
};

// Member access: obj.field
struct MemberAccessNode : ASTNode {
    ASTNodePtr object;
    std::string member;

    MemberAccessNode() : ASTNode(NodeType::MEMBER_ACCESS) {}
};

// Member set: obj.field = value
struct MemberSetNode : ASTNode {
    ASTNodePtr object;
    std::string member;
    ASTNodePtr value;

    MemberSetNode() : ASTNode(NodeType::MEMBER_SET) {}
};

// Index access: arr[i]
struct IndexAccessNode : ASTNode {
    ASTNodePtr object;
    ASTNodePtr index;

    IndexAccessNode() : ASTNode(NodeType::INDEX_ACCESS) {}
};

// Index set: arr[i] = val
struct IndexSetNode : ASTNode {
    ASTNodePtr object;
    ASTNodePtr index;
    ASTNodePtr value;

    IndexSetNode() : ASTNode(NodeType::INDEX_SET) {}
};

// Cast expression: (int) x, (string) y
struct CastNode : ASTNode {
    std::string targetType;
    ASTNodePtr expr;

    CastNode() : ASTNode(NodeType::CAST) {}
};

// New expression: new Player("Atlas")
struct NewExprNode : ASTNode {
    std::string className;
    std::vector<ASTNodePtr> arguments;
    std::vector<std::string> argNames;

    NewExprNode() : ASTNode(NodeType::NEW_EXPR) {}
};

// Lambda: (int x, int y) => x * y
struct LambdaNode : ASTNode {
    std::vector<Parameter> params;
    std::string returnType;
    ASTNodePtr body;   // can be a block or a single expression

    LambdaNode() : ASTNode(NodeType::LAMBDA) {}
};

// List literal: [1, 2, 3]
struct ListLiteralNode : ASTNode {
    std::vector<ASTNodePtr> elements;

    ListLiteralNode() : ASTNode(NodeType::LIST_LITERAL) {}
};

// Map literal: {"key": "value"} (for future use)
struct MapLiteralNode : ASTNode {
    std::vector<std::pair<ASTNodePtr, ASTNodePtr>> entries;

    MapLiteralNode() : ASTNode(NodeType::MAP_LITERAL) {}
};

// Struct initializer: { x: 0.0, y: -9.81, z: 0.0 }
struct StructInitNode : ASTNode {
    std::string structName;    // optional, may be inferred
    std::vector<std::pair<std::string, ASTNodePtr>> fields;

    StructInitNode() : ASTNode(NodeType::STRUCT_INIT) {}
};

// Ternary expression: cond ? trueExpr : falseExpr
struct TernaryNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr trueExpr;
    ASTNodePtr falseExpr;

    TernaryNode() : ASTNode(NodeType::TERNARY) {}
};

// ============================================================================
// Statements
// ============================================================================

// Expression statement: expr;
struct ExpressionStmtNode : ASTNode {
    ASTNodePtr expression;

    ExpressionStmtNode() : ASTNode(NodeType::EXPRESSION_STMT) {}
};

// Variable declaration: int x = 5;
struct VarDeclNode : ASTNode {
    std::string typeName;      // "int", "string", "Player", "List<int>", etc.
    std::string name;
    ASTNodePtr initializer;    // can be nullptr
    bool isConst = false;
    std::string accessModifier; // "public", "private", "protected", ""

    VarDeclNode() : ASTNode(NodeType::VAR_DECL) {}
};

// Block: { ... }
struct BlockNode : ASTNode {
    std::vector<ASTNodePtr> statements;

    BlockNode() : ASTNode(NodeType::BLOCK) {}
};

// If statement: if (cond) { ... } elif (cond) { ... } else { ... }
struct IfStmtNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr thenBranch;
    std::vector<std::pair<ASTNodePtr, ASTNodePtr>> elifBranches; // condition, body
    ASTNodePtr elseBranch; // nullptr if no else

    IfStmtNode() : ASTNode(NodeType::IF_STMT) {}
};

// Switch case clause
struct CaseClause {
    ASTNodePtr value;  // nullptr for default
    std::vector<ASTNodePtr> body;
    bool isDefault = false;
};

// Switch statement
struct SwitchStmtNode : ASTNode {
    ASTNodePtr expr;
    std::vector<CaseClause> cases;

    SwitchStmtNode() : ASTNode(NodeType::SWITCH_STMT) {}
};

// For loop: for (int i = 0; i < 10; i++) { ... }
struct ForStmtNode : ASTNode {
    ASTNodePtr initializer;
    ASTNodePtr condition;
    ASTNodePtr increment;
    ASTNodePtr body;

    ForStmtNode() : ASTNode(NodeType::FOR_STMT) {}
};

// For-each: for (string name in names) { ... }
struct ForEachStmtNode : ASTNode {
    std::string varType;
    std::string varName;
    ASTNodePtr iterable;
    ASTNodePtr body;

    ForEachStmtNode() : ASTNode(NodeType::FOR_EACH_STMT) {}
};

// While loop
struct WhileStmtNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr body;

    WhileStmtNode() : ASTNode(NodeType::WHILE_STMT) {}
};

// Do-while loop
struct DoWhileStmtNode : ASTNode {
    ASTNodePtr body;
    ASTNodePtr condition;

    DoWhileStmtNode() : ASTNode(NodeType::DO_WHILE_STMT) {}
};

// Break statement
struct BreakStmtNode : ASTNode {
    BreakStmtNode() : ASTNode(NodeType::BREAK_STMT) {}
};

// Continue statement
struct ContinueStmtNode : ASTNode {
    ContinueStmtNode() : ASTNode(NodeType::CONTINUE_STMT) {}
};

// Return statement
struct ReturnStmtNode : ASTNode {
    ASTNodePtr value; // can be nullptr

    ReturnStmtNode() : ASTNode(NodeType::RETURN_STMT) {}
};

// Function declaration
struct FuncDeclNode : ASTNode {
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;    // "" means default (int, returns 0)
    ASTNodePtr body;           // BlockNode
    std::string accessModifier; // "public", "private", "", etc.
    std::vector<std::string> genericParams; // e.g. <T, U>

    FuncDeclNode() : ASTNode(NodeType::FUNC_DECL) {}
};

// Class member (field or method)
struct ClassMember {
    std::string accessModifier; // "public", "private", "protected"
    bool isField = false;
    bool isStatic = false;
    // If field:
    std::string fieldType;
    std::string fieldName;
    ASTNodePtr fieldInit;
    // If method:
    ASTNodePtr method;  // FuncDeclNode
};

// Class declaration
struct ClassDeclNode : ASTNode {
    std::string name;
    std::string parentClass;                  // empty if no parent
    std::vector<std::string> interfaces;
    std::vector<ClassMember> members;

    ClassDeclNode() : ASTNode(NodeType::CLASS_DECL) {}
};

// Struct field
struct StructField {
    std::string typeName;
    std::string name;
};

// Struct declaration
struct StructDeclNode : ASTNode {
    std::string name;
    std::vector<StructField> fields;

    StructDeclNode() : ASTNode(NodeType::STRUCT_DECL) {}
};

// Enum member
struct EnumMember {
    std::string name;
    bool hasValue = false;
    int value = 0;
};

// Enum declaration
struct EnumDeclNode : ASTNode {
    std::string name;
    std::vector<EnumMember> members;

    EnumDeclNode() : ASTNode(NodeType::ENUM_DECL) {}
};

// Interface declaration
struct InterfaceDeclNode : ASTNode {
    std::string name;
    std::vector<ASTNodePtr> methods; // FuncDeclNode without bodies

    InterfaceDeclNode() : ASTNode(NodeType::INTERFACE_DECL) {}
};

// Import statement: import "file.lx"; import std.io;
struct ImportStmtNode : ASTNode {
    std::string path;       // "file.lx" or "std.io"
    bool isStdLib = false;

    ImportStmtNode() : ASTNode(NodeType::IMPORT_STMT) {}
};

// Export statement
struct ExportStmtNode : ASTNode {
    ASTNodePtr declaration;

    ExportStmtNode() : ASTNode(NodeType::EXPORT_STMT) {}
};

// Catch clause
struct CatchClause {
    std::string errorType;
    std::string errorName;
    ASTNodePtr body;
};

// Try-catch-finally
struct TryCatchNode : ASTNode {
    ASTNodePtr tryBody;
    std::vector<CatchClause> catchClauses;
    ASTNodePtr finallyBody; // can be nullptr

    TryCatchNode() : ASTNode(NodeType::TRY_CATCH) {}
};

// Throw statement
struct ThrowStmtNode : ASTNode {
    ASTNodePtr expr;

    ThrowStmtNode() : ASTNode(NodeType::THROW_STMT) {}
};

// Panic statement
struct PanicStmtNode : ASTNode {
    ASTNodePtr message;

    PanicStmtNode() : ASTNode(NodeType::PANIC_STMT) {}
};

// Pointer dereference assignment: *ptr = value
struct DerefAssignNode : ASTNode {
    ASTNodePtr pointer;  // The pointer expression (what is dereferenced)
    ASTNodePtr value;    // The value to assign
    Token op;            // The assignment operator (=, +=, etc.)

    DerefAssignNode() : ASTNode(NodeType::DEREF_ASSIGN) {}
};

// Inline assembly operand: "constraint"(expression)
struct AsmOperand {
    std::string constraint;  // e.g., "=r", "r"
    ASTNodePtr expr;         // The bound expression (variable or literal)
};

// Inline assembly: asm("instruction" : output : input : clobbers)
struct AsmStmtNode : ASTNode {
    std::string asmString;              // The assembly template string
    std::vector<AsmOperand> outputs;    // Output operands
    std::vector<AsmOperand> inputs;     // Input operands
    std::vector<std::string> clobbers;  // Clobber list (strings like "memory", "cc")
    bool isVolatile = true;

    AsmStmtNode() : ASTNode(NodeType::ASM_STMT) {}
};

// Unsafe block
struct UnsafeBlockNode : ASTNode {
    ASTNodePtr body;

    UnsafeBlockNode() : ASTNode(NodeType::UNSAFE_BLOCK) {}
};

// Cleanup statement
struct CleanupStmtNode : ASTNode {
    CleanupStmtNode() : ASTNode(NodeType::CLEANUP_STMT) {}
};

// Program (root node)
struct ProgramNode : ASTNode {
    std::vector<ASTNodePtr> declarations;

    ProgramNode() : ASTNode(NodeType::PROGRAM) {}
};
