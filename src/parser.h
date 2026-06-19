#pragma once

#include "token.h"
#include "ast.h"
#include <vector>
#include <string>
#include <memory>

// ============================================================================
// Flux Parser - Recursive descent parser producing an AST
// ============================================================================

class Parser {
public:
    Parser(const std::vector<Token>& tokens, const std::string& filename = "<stdin>");

    // Parse the token stream and return the program AST
    ASTNodePtr parse();

    const std::vector<std::string>& getErrors() const { return errors; }
    bool hasErrors() const { return !errors.empty(); }

private:
    std::vector<Token> tokens;
    std::string filename;
    int current = 0;
    std::vector<std::string> errors;

    // ========================================================================
    // Token navigation
    // ========================================================================
    Token peek() const;
    Token peekNext() const;
    Token previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchAny(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    bool isAtEnd() const;
    void error(const Token& token, const std::string& message);
    void synchronize();

    // ========================================================================
    // Type parsing helpers
    // ========================================================================
    bool isTypeToken() const;
    bool isTypeTokenAt(int pos) const;
    std::string parseTypeName();
    bool isAccessModifier() const;
    std::string parseAccessModifier();

    // ========================================================================
    // Declarations (top-level and inside blocks)
    // ========================================================================
    ASTNodePtr declaration();
    ASTNodePtr funcDeclaration(const std::string& access = "");
    ASTNodePtr classDeclaration();
    ASTNodePtr structDeclaration();
    ASTNodePtr enumDeclaration();
    ASTNodePtr interfaceDeclaration();
    ASTNodePtr importDeclaration();
    ASTNodePtr exportDeclaration();

    // ========================================================================
    // Statements
    // ========================================================================
    ASTNodePtr statement();
    ASTNodePtr varDeclaration(const std::string& typeName, const std::string& access = "",
                              bool explicitConst = false);
    ASTNodePtr blockStatement();
    ASTNodePtr statementOrBlock();   // block { ... } or single statement
    ASTNodePtr ifStatement();
    ASTNodePtr switchStatement();
    ASTNodePtr forStatement();
    ASTNodePtr whileStatement();
    ASTNodePtr doWhileStatement();
    ASTNodePtr returnStatement();
    ASTNodePtr breakStatement();
    ASTNodePtr continueStatement();
    ASTNodePtr tryCatchStatement();
    ASTNodePtr throwStatement();
    ASTNodePtr panicStatement();
    ASTNodePtr unsafeBlock();
    ASTNodePtr asmStatement();
    ASTNodePtr expressionStatement();

    // ========================================================================
    // Expressions (ordered by precedence, lowest to highest)
    // ========================================================================
    ASTNodePtr expression();
    ASTNodePtr assignment();
    ASTNodePtr ternary();
    ASTNodePtr logicOr();
    ASTNodePtr logicAnd();
    ASTNodePtr bitwiseOr();
    ASTNodePtr bitwiseXor();
    ASTNodePtr bitwiseAnd();
    ASTNodePtr butnotExpr();
    ASTNodePtr equality();
    ASTNodePtr comparison();
    ASTNodePtr shift();
    ASTNodePtr addition();
    ASTNodePtr multiplication();
    ASTNodePtr unary();
    ASTNodePtr postfix();
    ASTNodePtr call();
    ASTNodePtr primary();

    // ========================================================================
    // Helper parsers
    // ========================================================================
    std::vector<Parameter> parseParameters();
    std::vector<ASTNodePtr> parseArguments();
    ASTNodePtr finishCall(ASTNodePtr callee);
    bool checkLambda();
    ASTNodePtr parseLambda();
};
