#include "parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// ============================================================================
// Parser Implementation
// ============================================================================

Parser::Parser(const std::vector<Token>& tokens, const std::string& filename)
    : tokens(tokens), filename(filename) {}

ASTNodePtr Parser::parse() {
    auto program = std::make_shared<ProgramNode>();
    while (!isAtEnd()) {
        try {
            auto decl = declaration();
            if (decl) {
                program->declarations.push_back(decl);
            }
        } catch (const std::runtime_error& e) {
            errors.push_back(e.what());
            synchronize();
        }
    }
    return program;
}

// ============================================================================
// Token navigation
// ============================================================================

Token Parser::peek() const {
    return tokens[current];
}

Token Parser::peekNext() const {
    if (current + 1 < (int)tokens.size()) return tokens[current + 1];
    return tokens.back();
}

Token Parser::previous() const {
    return tokens[current - 1];
}

Token Parser::advance() {
    if (!isAtEnd()) current++;
    return tokens[current - 1];
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    error(peek(), message);
    throw std::runtime_error(errors.back());
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::TOKEN_EOF;
}

void Parser::error(const Token& token, const std::string& message) {
    std::ostringstream oss;
    oss << filename << ":" << token.line << ":" << token.column
        << ": error: " << message;
    if (token.type != TokenType::TOKEN_EOF) {
        oss << " (near '" << token.lexeme << "')";
    }
    errors.push_back(oss.str());
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        if (previous().type == TokenType::RIGHT_BRACE) return;
        switch (peek().type) {
            case TokenType::KW_FUNC:
            case TokenType::KW_CLASS:
            case TokenType::KW_STRUCT:
            case TokenType::KW_ENUM:
            case TokenType::KW_IF:
            case TokenType::KW_FOR:
            case TokenType::KW_WHILE:
            case TokenType::KW_RETURN:
            case TokenType::KW_IMPORT:
            case TokenType::KW_EXPORT:
            case TokenType::KW_TRY:
                return;
            default:
                break;
        }
        advance();
    }
}

// ============================================================================
// Type parsing helpers
// ============================================================================

bool Parser::isTypeToken() const {
    return isTypeKeyword(peek().type) || peek().type == TokenType::IDENTIFIER;
}

bool Parser::isTypeTokenAt(int pos) const {
    if (pos >= (int)tokens.size()) return false;
    return isTypeKeyword(tokens[pos].type) || tokens[pos].type == TokenType::IDENTIFIER;
}

std::string Parser::parseTypeName() {
    std::string typeName;
    if (isTypeKeyword(peek().type)) {
        typeName = advance().lexeme;
    } else if (check(TokenType::IDENTIFIER)) {
        typeName = advance().lexeme;
    } else if (check(TokenType::KW_FUNC)) {
        // 'func' as a type — function pointer / callback type
        typeName = advance().lexeme; // "func"
    } else {
        error(peek(), "Expected type name");
        throw std::runtime_error(errors.back());
    }

    // Handle generic types: List<int>, Map<string, int>
    if (check(TokenType::LESS)) {
        typeName += "<";
        advance(); // consume <
        typeName += parseTypeName();
        while (match(TokenType::COMMA)) {
            typeName += ", ";
            typeName += parseTypeName();
        }
        consume(TokenType::GREATER, "Expected '>' after generic type parameters");
        typeName += ">";
    }

    // Handle array type: int[], byte[], int[256]
    if (check(TokenType::LEFT_BRACKET) && current + 1 < (int)tokens.size()) {
        if (tokens[current + 1].type == TokenType::RIGHT_BRACKET) {
            advance(); advance(); // consume []
            typeName += "[]";
        } else if (tokens[current + 1].type == TokenType::INT_LITERAL &&
                   current + 2 < (int)tokens.size() &&
                   tokens[current + 2].type == TokenType::RIGHT_BRACKET) {
            advance(); // consume [
            std::string size = std::to_string(peek().intValue);
            advance(); // consume size
            advance(); // consume ]
            typeName += "[" + size + "]";
        }
    }

    // Handle pointer types: int*, byte*, etc.
    while (check(TokenType::STAR)) {
        advance();
        typeName += "*";
    }

    // Handle nullable: int?
    // (We'll handle this in the future if needed)

    return typeName;
}

bool Parser::isAccessModifier() const {
    return check(TokenType::KW_PUBLIC) || check(TokenType::KW_PRIVATE) ||
           check(TokenType::KW_PROTECTED);
}

std::string Parser::parseAccessModifier() {
    if (match(TokenType::KW_PUBLIC)) return "public";
    if (match(TokenType::KW_PRIVATE)) return "private";
    if (match(TokenType::KW_PROTECTED)) return "protected";
    return "";
}

// ============================================================================
// Declarations
// ============================================================================

ASTNodePtr Parser::declaration() {
    // Handle access modifiers before declarations
    std::string access = "";
    if (isAccessModifier()) {
        access = parseAccessModifier();
    }

    // Handle 'const' keyword: const type name = value;
    bool isConst = false;
    if (check(TokenType::KW_CONST)) {
        isConst = true;
        advance(); // consume 'const'
    }

    if (!isConst && check(TokenType::KW_FUNC)) {
        // Disambiguate: func name = value; (var decl) vs func name(...) (func decl)
        if (current + 1 < (int)tokens.size() && tokens[current + 1].type == TokenType::IDENTIFIER &&
            current + 2 < (int)tokens.size() &&
            (tokens[current + 2].type == TokenType::EQUAL || tokens[current + 2].type == TokenType::SEMICOLON)) {
            // This is a variable of type 'func'
            std::string typeName = parseTypeName(); // consumes "func"
            return varDeclaration(typeName, access, isConst);
        }
        return funcDeclaration(access);
    }
    if (!isConst && check(TokenType::KW_CLASS)) return classDeclaration();
    if (!isConst && check(TokenType::KW_STRUCT)) return structDeclaration();
    if (!isConst && check(TokenType::KW_ENUM)) return enumDeclaration();
    if (!isConst && check(TokenType::KW_INTERFACE)) return interfaceDeclaration();
    if (!isConst && check(TokenType::KW_IMPORT)) return importDeclaration();
    if (!isConst && check(TokenType::KW_EXPORT)) return exportDeclaration();

    // Check for variable declaration: type identifier ...
    if (isTypeToken() && current + 1 < (int)tokens.size()) {
        // Check if next token is IDENTIFIER (simple type), < (generic type),
        // * (pointer type), or [ (array type)
        TokenType nextType = tokens[current + 1].type;
        if (nextType == TokenType::IDENTIFIER || nextType == TokenType::LESS ||
            nextType == TokenType::STAR || nextType == TokenType::LEFT_BRACKET) {
            // Might also be: List<int> myList (generic type before identifier)
            // Or: Player hero (class type before identifier)
            // Or: int* ptr (pointer type before identifier)
            // Need to ensure it's not something like: myFunc(...)
            int saved = current;
            std::string typeName = parseTypeName();

            if (check(TokenType::IDENTIFIER)) {
                return varDeclaration(typeName, access, isConst);
            }

            // Not a var decl, backtrack
            current = saved;
        }
    }

    return statement();
}

ASTNodePtr Parser::funcDeclaration(const std::string& access) {
    consume(TokenType::KW_FUNC, "Expected 'func'");

    std::string name = consume(TokenType::IDENTIFIER, "Expected function name").lexeme;

    // Generic params: func myFunc<T>(...)
    std::vector<std::string> genericParams;
    if (match(TokenType::LESS)) {
        do {
            genericParams.push_back(
                consume(TokenType::IDENTIFIER, "Expected generic type parameter").lexeme);
        } while (match(TokenType::COMMA));
        consume(TokenType::GREATER, "Expected '>' after generic parameters");
    }

    consume(TokenType::LEFT_PAREN, "Expected '(' after function name");
    std::vector<Parameter> params = parseParameters();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");

    // Return type: -> type
    std::string returnType = "";
    if (match(TokenType::ARROW)) {
        returnType = parseTypeName();
    }

    ASTNodePtr body = blockStatement();

    auto node = std::make_shared<FuncDeclNode>();
    node->name = name;
    node->params = params;
    node->returnType = returnType;
    node->body = body;
    node->accessModifier = access;
    node->genericParams = genericParams;
    node->line = previous().line;
    return node;
}

ASTNodePtr Parser::classDeclaration() {
    consume(TokenType::KW_CLASS, "Expected 'class'");
    std::string name = consume(TokenType::IDENTIFIER, "Expected class name").lexeme;

    std::string parentClass = "";
    if (match(TokenType::KW_EXTENDS)) {
        parentClass = consume(TokenType::IDENTIFIER, "Expected parent class name").lexeme;
    }

    std::vector<std::string> interfaces;
    if (match(TokenType::KW_IMPLEMENTS)) {
        do {
            interfaces.push_back(
                consume(TokenType::IDENTIFIER, "Expected interface name").lexeme);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::LEFT_BRACE, "Expected '{' before class body");

    auto node = std::make_shared<ClassDeclNode>();
    node->name = name;
    node->parentClass = parentClass;
    node->interfaces = interfaces;
    node->line = previous().line;

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        ClassMember member;
        member.accessModifier = parseAccessModifier();

        // Check for 'static' modifier
        if (check(TokenType::KW_STATIC)) {
            member.isStatic = true;
            advance(); // consume 'static'
        }

        if (check(TokenType::KW_FUNC)) {
            // Method
            member.isField = false;
            member.method = funcDeclaration(member.accessModifier);
            node->members.push_back(member);
        } else if (isTypeToken()) {
            // Field
            member.isField = true;
            std::string typeName = parseTypeName();
            member.fieldType = typeName;
            member.fieldName = consume(TokenType::IDENTIFIER, "Expected field name").lexeme;
            if (match(TokenType::EQUAL)) {
                member.fieldInit = expression();
            }
            consume(TokenType::SEMICOLON, "Expected ';' after field declaration");
            node->members.push_back(member);
        } else {
            error(peek(), "Expected field or method declaration inside class");
            advance();
        }
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after class body");
    return node;
}

ASTNodePtr Parser::structDeclaration() {
    consume(TokenType::KW_STRUCT, "Expected 'struct'");
    std::string name = consume(TokenType::IDENTIFIER, "Expected struct name").lexeme;

    consume(TokenType::LEFT_BRACE, "Expected '{' before struct body");

    auto node = std::make_shared<StructDeclNode>();
    node->name = name;
    node->line = previous().line;

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        std::string typeName = parseTypeName();
        std::string fieldName = consume(TokenType::IDENTIFIER, "Expected field name").lexeme;
        consume(TokenType::SEMICOLON, "Expected ';' after struct field");
        node->fields.push_back({typeName, fieldName});
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after struct body");
    return node;
}

ASTNodePtr Parser::enumDeclaration() {
    consume(TokenType::KW_ENUM, "Expected 'enum'");
    std::string name = consume(TokenType::IDENTIFIER, "Expected enum name").lexeme;

    consume(TokenType::LEFT_BRACE, "Expected '{' before enum body");

    auto node = std::make_shared<EnumDeclNode>();
    node->name = name;
    node->line = previous().line;

    int nextValue = 0;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        EnumMember member;
        member.name = consume(TokenType::IDENTIFIER, "Expected enum member name").lexeme;

        if (match(TokenType::EQUAL)) {
            Token valToken = consume(TokenType::INT_LITERAL, "Expected integer value for enum member");
            member.value = (int)valToken.intValue;
            member.hasValue = true;
            nextValue = member.value + 1;
        } else {
            member.value = nextValue++;
            member.hasValue = false;
        }

        node->members.push_back(member);

        if (!check(TokenType::RIGHT_BRACE)) {
            // Allow trailing comma
            match(TokenType::COMMA);
        }
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after enum body");
    return node;
}

ASTNodePtr Parser::interfaceDeclaration() {
    consume(TokenType::KW_INTERFACE, "Expected 'interface'");
    std::string name = consume(TokenType::IDENTIFIER, "Expected interface name").lexeme;

    consume(TokenType::LEFT_BRACE, "Expected '{' before interface body");

    auto node = std::make_shared<InterfaceDeclNode>();
    node->name = name;
    node->line = previous().line;

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        // Interface methods have no body: func methodName(params) -> type;
        consume(TokenType::KW_FUNC, "Expected 'func' in interface");
        std::string methodName = consume(TokenType::IDENTIFIER, "Expected method name").lexeme;

        consume(TokenType::LEFT_PAREN, "Expected '('");
        auto params = parseParameters();
        consume(TokenType::RIGHT_PAREN, "Expected ')'");

        std::string returnType = "";
        if (match(TokenType::ARROW)) {
            returnType = parseTypeName();
        }

        consume(TokenType::SEMICOLON, "Expected ';' after interface method declaration");

        auto funcNode = std::make_shared<FuncDeclNode>();
        funcNode->name = methodName;
        funcNode->params = params;
        funcNode->returnType = returnType;
        funcNode->body = nullptr; // no body for interface methods
        node->methods.push_back(funcNode);
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after interface body");
    return node;
}

ASTNodePtr Parser::importDeclaration() {
    consume(TokenType::KW_IMPORT, "Expected 'import'");

    auto node = std::make_shared<ImportStmtNode>();
    node->line = previous().line;

    if (check(TokenType::STRING_LITERAL)) {
        // import "file.lx";
        Token path = advance();
        node->path = path.stringValue;
        node->isStdLib = false;
    } else {
        // import std.io;
        std::string path = consume(TokenType::IDENTIFIER, "Expected module name").lexeme;
        while (match(TokenType::DOT)) {
            path += ".";
            path += consume(TokenType::IDENTIFIER, "Expected module component").lexeme;
        }
        node->path = path;
        node->isStdLib = (path.find("std.") == 0);
    }

    consume(TokenType::SEMICOLON, "Expected ';' after import");
    return node;
}

ASTNodePtr Parser::exportDeclaration() {
    consume(TokenType::KW_EXPORT, "Expected 'export'");

    auto node = std::make_shared<ExportStmtNode>();
    node->line = previous().line;
    node->declaration = declaration();
    return node;
}

// ============================================================================
// Statements
// ============================================================================

ASTNodePtr Parser::statement() {
    if (check(TokenType::LEFT_BRACE)) return blockStatement();
    if (check(TokenType::KW_IF)) return ifStatement();
    if (check(TokenType::KW_SWITCH)) return switchStatement();
    if (check(TokenType::KW_FOR)) return forStatement();
    if (check(TokenType::KW_WHILE)) return whileStatement();
    if (check(TokenType::KW_DO)) return doWhileStatement();
    if (check(TokenType::KW_RETURN)) return returnStatement();
    if (check(TokenType::KW_BREAK)) return breakStatement();
    if (check(TokenType::KW_CONTINUE)) return continueStatement();
    if (check(TokenType::KW_TRY)) return tryCatchStatement();
    if (check(TokenType::KW_THROW)) return throwStatement();
    if (check(TokenType::KW_PANIC)) return panicStatement();
    if (check(TokenType::KW_UNSAFE)) return unsafeBlock();
    if (check(TokenType::KW_ASM)) return asmStatement();
    if (check(TokenType::KW_CLEANUP)) {
        advance();
        consume(TokenType::SEMICOLON, "Expected ';' after cleanup");
        auto node = std::make_shared<CleanupStmtNode>();
        node->line = previous().line;
        return node;
    }

    return expressionStatement();
}

ASTNodePtr Parser::varDeclaration(const std::string& typeName, const std::string& access,
                                  bool explicitConst) {
    auto node = std::make_shared<VarDeclNode>();
    node->typeName = typeName;
    node->name = consume(TokenType::IDENTIFIER, "Expected variable name").lexeme;
    node->accessModifier = access;
    node->line = previous().line;

    // Handle C-style array declaration: type name[size]; → type becomes type[size]
    if (check(TokenType::LEFT_BRACKET)) {
        advance(); // consume [
        if (check(TokenType::INT_LITERAL)) {
            std::string size = std::to_string(peek().intValue);
            advance(); // consume size
            consume(TokenType::RIGHT_BRACKET, "Expected ']' after array size");
            node->typeName = typeName + "[" + size + "]";
        } else if (check(TokenType::RIGHT_BRACKET)) {
            advance(); // consume ]
            node->typeName = typeName + "[]";
        } else {
            consume(TokenType::RIGHT_BRACKET, "Expected ']' after array size");
        }
    }

    // Explicit const keyword takes priority
    if (explicitConst) {
        node->isConst = true;
    } else {
        // Check for UPPER_SNAKE_CASE -> constant
        bool allUpper = true;
        for (char c : node->name) {
            if (c != '_' && !std::isupper(c) && !std::isdigit(c)) {
                allUpper = false;
                break;
            }
        }
        if (allUpper && node->name.length() > 1) {
            node->isConst = true;
        }
    }

    if (match(TokenType::EQUAL)) {
        node->initializer = expression();
    }

    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

ASTNodePtr Parser::blockStatement() {
    consume(TokenType::LEFT_BRACE, "Expected '{'");

    auto block = std::make_shared<BlockNode>();
    block->line = previous().line;

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto decl = declaration();
        if (decl) block->statements.push_back(decl);
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}'");
    return block;
}

ASTNodePtr Parser::statementOrBlock() {
    // Accept either a brace-enclosed block or a single statement
    if (check(TokenType::LEFT_BRACE)) {
        return blockStatement();
    }
    // Single statement — wrap in a synthetic block for AST consistency
    auto block = std::make_shared<BlockNode>();
    block->line = peek().line;
    auto stmt = declaration(); // declaration() also handles var decls inside blocks
    if (stmt) block->statements.push_back(stmt);
    return block;
}

ASTNodePtr Parser::ifStatement() {
    consume(TokenType::KW_IF, "Expected 'if'");
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'if'");
    ASTNodePtr condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after condition");

    ASTNodePtr thenBranch = statementOrBlock();

    auto node = std::make_shared<IfStmtNode>();
    node->condition = condition;
    node->thenBranch = thenBranch;
    node->line = previous().line;

    // Parse elif chains
    while (check(TokenType::KW_ELIF) ||
           (check(TokenType::KW_ELSE) && current + 1 < (int)tokens.size() &&
            tokens[current + 1].type == TokenType::KW_IF)) {
        if (check(TokenType::KW_ELIF)) {
            advance(); // consume elif
        } else {
            advance(); // consume else
            advance(); // consume if
        }
        consume(TokenType::LEFT_PAREN, "Expected '(' after 'elif'");
        ASTNodePtr elifCond = expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after condition");
        ASTNodePtr elifBody = statementOrBlock();
        node->elifBranches.push_back({elifCond, elifBody});
    }

    // Parse else
    if (match(TokenType::KW_ELSE)) {
        node->elseBranch = statementOrBlock();
    }

    return node;
}

ASTNodePtr Parser::switchStatement() {
    consume(TokenType::KW_SWITCH, "Expected 'switch'");
    consume(TokenType::LEFT_PAREN, "Expected '('");
    ASTNodePtr expr = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')'");
    consume(TokenType::LEFT_BRACE, "Expected '{'");

    auto node = std::make_shared<SwitchStmtNode>();
    node->expr = expr;
    node->line = previous().line;

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        CaseClause clause;
        if (match(TokenType::KW_CASE)) {
            clause.value = expression();
            consume(TokenType::COLON, "Expected ':' after case value");
            clause.isDefault = false;
        } else if (match(TokenType::KW_DEFAULT)) {
            consume(TokenType::COLON, "Expected ':' after 'default'");
            clause.isDefault = true;
        } else {
            error(peek(), "Expected 'case' or 'default'");
            advance();
            continue;
        }

        // Parse case body until next case, default, or closing brace
        while (!check(TokenType::KW_CASE) && !check(TokenType::KW_DEFAULT) &&
               !check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
            clause.body.push_back(declaration());
        }

        node->cases.push_back(clause);
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}'");
    return node;
}

ASTNodePtr Parser::forStatement() {
    consume(TokenType::KW_FOR, "Expected 'for'");
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'for'");

    // Determine if for-each or C-style for
    // For-each: for (type name in expr)
    // C-style:  for (init; cond; incr)
    if (isTypeToken() && current + 1 < (int)tokens.size()) {
        TokenType nextType = tokens[current + 1].type;
        if (nextType == TokenType::IDENTIFIER || nextType == TokenType::LESS ||
            nextType == TokenType::STAR || nextType == TokenType::LEFT_BRACKET) {
            // Look ahead to see if there's an 'in' keyword after the identifier
            int saved = current;
            std::string typeName = parseTypeName();

            if (check(TokenType::IDENTIFIER)) {
                std::string varName = advance().lexeme;

                if (check(TokenType::KW_IN)) {
                    // For-each loop
                    advance(); // consume 'in'
                    ASTNodePtr iterable = expression();
                    consume(TokenType::RIGHT_PAREN, "Expected ')'");
                    ASTNodePtr body = statementOrBlock();

                    auto node = std::make_shared<ForEachStmtNode>();
                    node->varType = typeName;
                    node->varName = varName;
                    node->iterable = iterable;
                    node->body = body;
                    node->line = previous().line;
                    return node;
                }

                // Not for-each; backtrack and parse as C-style for
                current = saved;
            } else {
                current = saved;
            }
        }
    }

    // C-style for loop: for (init; cond; incr)
    ASTNodePtr initializer;
    if (match(TokenType::SEMICOLON)) {
        initializer = nullptr;
    } else if (isTypeToken() && current + 1 < (int)tokens.size() &&
               (tokens[current + 1].type == TokenType::IDENTIFIER ||
                tokens[current + 1].type == TokenType::STAR ||
                tokens[current + 1].type == TokenType::LEFT_BRACKET)) {
        std::string typeName = parseTypeName();
        initializer = varDeclaration(typeName);
        // varDeclaration already consumed the semicolon
    } else {
        initializer = expression();
        consume(TokenType::SEMICOLON, "Expected ';' after for initializer");
    }

    ASTNodePtr condition = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after for condition");

    ASTNodePtr increment = nullptr;
    if (!check(TokenType::RIGHT_PAREN)) {
        increment = expression();
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after for clauses");
    ASTNodePtr body = statementOrBlock();

    auto node = std::make_shared<ForStmtNode>();
    node->initializer = initializer;
    node->condition = condition;
    node->increment = increment;
    node->body = body;
    node->line = previous().line;
    return node;
}

ASTNodePtr Parser::whileStatement() {
    consume(TokenType::KW_WHILE, "Expected 'while'");
    consume(TokenType::LEFT_PAREN, "Expected '('");
    ASTNodePtr condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')'");
    ASTNodePtr body = statementOrBlock();

    auto node = std::make_shared<WhileStmtNode>();
    node->condition = condition;
    node->body = body;
    node->line = previous().line;
    return node;
}

ASTNodePtr Parser::doWhileStatement() {
    consume(TokenType::KW_DO, "Expected 'do'");
    ASTNodePtr body = statementOrBlock();
    consume(TokenType::KW_WHILE, "Expected 'while' after do block");
    consume(TokenType::LEFT_PAREN, "Expected '('");
    ASTNodePtr condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')'");
    consume(TokenType::SEMICOLON, "Expected ';' after do-while");

    auto node = std::make_shared<DoWhileStmtNode>();
    node->body = body;
    node->condition = condition;
    node->line = previous().line;
    return node;
}

ASTNodePtr Parser::returnStatement() {
    consume(TokenType::KW_RETURN, "Expected 'return'");
    auto node = std::make_shared<ReturnStmtNode>();
    node->line = previous().line;

    if (!check(TokenType::SEMICOLON)) {
        node->value = expression();
    }

    consume(TokenType::SEMICOLON, "Expected ';' after return value");
    return node;
}

ASTNodePtr Parser::breakStatement() {
    consume(TokenType::KW_BREAK, "Expected 'break'");
    consume(TokenType::SEMICOLON, "Expected ';' after 'break'");
    auto node = std::make_shared<BreakStmtNode>();
    node->line = previous().line;
    return node;
}

ASTNodePtr Parser::continueStatement() {
    consume(TokenType::KW_CONTINUE, "Expected 'continue'");
    consume(TokenType::SEMICOLON, "Expected ';' after 'continue'");
    auto node = std::make_shared<ContinueStmtNode>();
    node->line = previous().line;
    return node;
}

ASTNodePtr Parser::tryCatchStatement() {
    consume(TokenType::KW_TRY, "Expected 'try'");
    ASTNodePtr tryBody = blockStatement();

    auto node = std::make_shared<TryCatchNode>();
    node->tryBody = tryBody;
    node->line = previous().line;

    while (check(TokenType::KW_CATCH)) {
        advance(); // consume 'catch'
        consume(TokenType::LEFT_PAREN, "Expected '(' after 'catch'");

        CatchClause clause;
        // catch (ErrorType varName)
        clause.errorType = parseTypeName();
        clause.errorName = consume(TokenType::IDENTIFIER, "Expected error variable name").lexeme;

        consume(TokenType::RIGHT_PAREN, "Expected ')'");
        clause.body = blockStatement();
        node->catchClauses.push_back(clause);
    }

    if (match(TokenType::KW_FINALLY)) {
        node->finallyBody = blockStatement();
    }

    return node;
}

ASTNodePtr Parser::throwStatement() {
    consume(TokenType::KW_THROW, "Expected 'throw'");
    auto node = std::make_shared<ThrowStmtNode>();
    node->line = previous().line;
    node->expr = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after throw expression");
    return node;
}

ASTNodePtr Parser::panicStatement() {
    consume(TokenType::KW_PANIC, "Expected 'panic'");
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'panic'");
    auto node = std::make_shared<PanicStmtNode>();
    node->line = previous().line;
    node->message = expression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after panic message");
    consume(TokenType::SEMICOLON, "Expected ';' after panic");
    return node;
}

ASTNodePtr Parser::unsafeBlock() {
    consume(TokenType::KW_UNSAFE, "Expected 'unsafe'");
    auto node = std::make_shared<UnsafeBlockNode>();
    node->line = previous().line;
    node->body = blockStatement();
    return node;
}

ASTNodePtr Parser::asmStatement() {
    consume(TokenType::KW_ASM, "Expected 'asm'");
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'asm'");

    auto node = std::make_shared<AsmStmtNode>();
    node->line = previous().line;

    // Parse the assembly template string
    Token asmStr = consume(TokenType::STRING_LITERAL, "Expected assembly string");
    node->asmString = asmStr.stringValue;

    // Parse optional output, input, and clobber constraints
    // GCC extended asm format: asm("template" : outputs : inputs : clobbers)
    // Each output/input is: "constraint"(expression) [, ...]
    // Clobbers are: "reg" [, ...]
    if (match(TokenType::COLON)) {
        // Output operands
        if (!check(TokenType::COLON) && !check(TokenType::RIGHT_PAREN)) {
            do {
                AsmOperand op;
                Token constraint = consume(TokenType::STRING_LITERAL, "Expected constraint string");
                op.constraint = constraint.stringValue;
                consume(TokenType::LEFT_PAREN, "Expected '(' after constraint");
                op.expr = expression();
                consume(TokenType::RIGHT_PAREN, "Expected ')' after operand");
                node->outputs.push_back(op);
            } while (match(TokenType::COMMA));
        }
        if (match(TokenType::COLON)) {
            // Input operands
            if (!check(TokenType::COLON) && !check(TokenType::RIGHT_PAREN)) {
                do {
                    AsmOperand op;
                    Token constraint = consume(TokenType::STRING_LITERAL, "Expected constraint string");
                    op.constraint = constraint.stringValue;
                    consume(TokenType::LEFT_PAREN, "Expected '(' after constraint");
                    op.expr = expression();
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after operand");
                    node->inputs.push_back(op);
                } while (match(TokenType::COMMA));
            }
            if (match(TokenType::COLON)) {
                // Clobber list: "memory", "cc", etc.
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        Token clob = consume(TokenType::STRING_LITERAL, "Expected clobber string");
                        node->clobbers.push_back(clob.stringValue);
                    } while (match(TokenType::COMMA));
                }
            }
        }
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after asm arguments");
    consume(TokenType::SEMICOLON, "Expected ';' after asm statement");
    return node;
}

ASTNodePtr Parser::expressionStatement() {
    ASTNodePtr expr = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression");

    auto node = std::make_shared<ExpressionStmtNode>();
    node->expression = expr;
    node->line = previous().line;
    return node;
}

// ============================================================================
// Expressions
// ============================================================================

ASTNodePtr Parser::expression() {
    return assignment();
}

ASTNodePtr Parser::ternary() {
    ASTNodePtr expr = logicOr();

    if (match(TokenType::QUESTION)) {
        auto node = std::make_shared<TernaryNode>();
        node->condition = expr;
        node->line = previous().line;
        node->trueExpr = ternary();  // right-associative
        consume(TokenType::COLON, "Expected ':' in ternary expression");
        node->falseExpr = ternary();
        return node;
    }

    return expr;
}

ASTNodePtr Parser::assignment() {
    ASTNodePtr expr = ternary();

    if (matchAny({TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL,
                  TokenType::STAR_EQUAL, TokenType::SLASH_EQUAL, TokenType::PERCENT_EQUAL,
                  TokenType::AMP_EQUAL, TokenType::PIPE_EQUAL, TokenType::CARET_EQUAL,
                  TokenType::LEFT_SHIFT_EQUAL, TokenType::RIGHT_SHIFT_EQUAL})) {
        Token op = previous();

        // Special case: type redefinition  (var = type = value)
        // identifier = type_keyword = value
        // or: identifier = type_keyword;   (only for built-in type keywords)
        if (op.type == TokenType::EQUAL && isTypeToken() && expr->nodeType == NodeType::VARIABLE) {
            auto varNode = std::static_pointer_cast<VariableNode>(expr);

            // For identifiers (user-defined types), only allow type redef
            // if followed by another '=' (i.e., var = Type = value).
            // For built-in type keywords, also allow var = type;
            bool isBuiltinType = isTypeKeyword(peek().type);

            // Check if next token after the type keyword is '=' or ';'
            int saved = current;
            std::string newType = parseTypeName();

            if (check(TokenType::EQUAL)) {
                // var = type = value  (full type redef)
                advance(); // consume second =
                ASTNodePtr value = expression();

                auto redef = std::make_shared<TypeRedefNode>();
                redef->name = varNode->name;
                redef->newType = newType;
                redef->value = value;
                redef->line = op.line;
                return redef;
            } else if (isBuiltinType && check(TokenType::SEMICOLON)) {
                // var = type;  (type redef, keep value auto-converted)
                // Only for built-in types to avoid ambiguity with var = expr;
                auto redef = std::make_shared<TypeRedefNode>();
                redef->name = varNode->name;
                redef->newType = newType;
                redef->value = nullptr;
                redef->line = op.line;
                return redef;
            }

            // Not a type redef, backtrack
            current = saved;
        }

        ASTNodePtr value = assignment();

        if (expr->nodeType == NodeType::VARIABLE) {
            auto varNode = std::static_pointer_cast<VariableNode>(expr);
            auto assign = std::make_shared<AssignNode>();
            assign->name = varNode->name;
            assign->op = op;
            assign->value = value;
            assign->line = op.line;
            return assign;
        } else if (expr->nodeType == NodeType::MEMBER_ACCESS) {
            auto getNode = std::static_pointer_cast<MemberAccessNode>(expr);
            auto setNode = std::make_shared<MemberSetNode>();
            setNode->object = getNode->object;
            setNode->member = getNode->member;
            setNode->value = value;
            setNode->line = op.line;
            return setNode;
        } else if (expr->nodeType == NodeType::INDEX_ACCESS) {
            auto idxNode = std::static_pointer_cast<IndexAccessNode>(expr);
            auto setNode = std::make_shared<IndexSetNode>();
            setNode->object = idxNode->object;
            setNode->index = idxNode->index;
            setNode->value = value;
            setNode->line = op.line;
            return setNode;
        } else if (expr->nodeType == NodeType::UNARY) {
            // Pointer dereference assignment: *ptr = value;
            auto unaryNode = std::static_pointer_cast<UnaryNode>(expr);
            if (unaryNode->op.type == TokenType::STAR && unaryNode->isPrefix) {
                auto derefAssign = std::make_shared<DerefAssignNode>();
                derefAssign->pointer = unaryNode->operand;
                derefAssign->value = value;
                derefAssign->op = op;
                derefAssign->line = op.line;
                return derefAssign;
            }
        }

        error(op, "Invalid assignment target");
    }

    return expr;
}

ASTNodePtr Parser::logicOr() {
    ASTNodePtr left = logicAnd();

    while (match(TokenType::PIPE_PIPE)) {
        Token op = previous();
        ASTNodePtr right = logicAnd();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::logicAnd() {
    ASTNodePtr left = bitwiseOr();

    while (match(TokenType::AMP_AMP)) {
        Token op = previous();
        ASTNodePtr right = bitwiseOr();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::bitwiseOr() {
    ASTNodePtr left = bitwiseXor();

    while (match(TokenType::PIPE)) {
        Token op = previous();
        ASTNodePtr right = bitwiseXor();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::bitwiseXor() {
    ASTNodePtr left = bitwiseAnd();

    while (match(TokenType::CARET)) {
        Token op = previous();
        ASTNodePtr right = bitwiseAnd();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::bitwiseAnd() {
    ASTNodePtr left = butnotExpr();

    while (match(TokenType::AMPERSAND)) {
        Token op = previous();
        ASTNodePtr right = butnotExpr();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::butnotExpr() {
    ASTNodePtr left = equality();

    while (match(TokenType::KW_BUTNOT)) {
        Token op = previous();
        ASTNodePtr right = equality();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::equality() {
    ASTNodePtr left = comparison();

    while (matchAny({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL,
                     TokenType::EQUAL_NUM_EQUAL, TokenType::EQUAL_WORD_EQUAL})) {
        Token op = previous();
        ASTNodePtr right = comparison();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::comparison() {
    ASTNodePtr left = shift();

    while (matchAny({TokenType::LESS, TokenType::LESS_EQUAL,
                     TokenType::GREATER, TokenType::GREATER_EQUAL})) {
        Token op = previous();
        ASTNodePtr right = shift();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::shift() {
    ASTNodePtr left = addition();

    while (matchAny({TokenType::LEFT_SHIFT, TokenType::RIGHT_SHIFT})) {
        Token op = previous();
        ASTNodePtr right = addition();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::addition() {
    ASTNodePtr left = multiplication();

    while (matchAny({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        ASTNodePtr right = multiplication();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::multiplication() {
    ASTNodePtr left = unary();

    while (matchAny({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        ASTNodePtr right = unary();
        auto node = std::make_shared<BinaryNode>();
        node->left = left;
        node->op = op;
        node->right = right;
        node->line = op.line;
        left = node;
    }

    return left;
}

ASTNodePtr Parser::unary() {
    // Prefix: !, -, ++, --, ~
    if (matchAny({TokenType::BANG, TokenType::MINUS, TokenType::PLUS_PLUS,
                  TokenType::MINUS_MINUS, TokenType::TILDE, TokenType::STAR,
                  TokenType::AMPERSAND})) {
        Token op = previous();
        ASTNodePtr operand = unary();
        auto node = std::make_shared<UnaryNode>();
        node->op = op;
        node->operand = operand;
        node->isPrefix = true;
        node->line = op.line;
        return node;
    }

    // Cast: (type) expr
    if (check(TokenType::LEFT_PAREN) && current + 1 < (int)tokens.size() &&
        isTypeTokenAt(current + 1)) {
        // Check if this is actually a cast: (type) followed by an expression
        // vs a grouped expression: (expr)
        int saved = current;
        advance(); // consume (
        if (isTypeToken()) {
            std::string typeName = parseTypeName();
            if (check(TokenType::RIGHT_PAREN)) {
                advance(); // consume )
                ASTNodePtr expr = unary();
                auto cast = std::make_shared<CastNode>();
                cast->targetType = typeName;
                cast->expr = expr;
                cast->line = previous().line;
                return cast;
            }
        }
        current = saved; // not a cast, backtrack
    }

    return postfix();
}

ASTNodePtr Parser::postfix() {
    ASTNodePtr expr = call();

    while (matchAny({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        Token op = previous();
        auto node = std::make_shared<PostfixNode>();
        node->operand = expr;
        node->op = op;
        node->line = op.line;
        expr = node;
    }

    return expr;
}

ASTNodePtr Parser::call() {
    ASTNodePtr expr = primary();

    while (true) {
        if (match(TokenType::LEFT_PAREN)) {
            // Function call
            expr = finishCall(expr);
        } else if (match(TokenType::DOT)) {
            // Member access — allow keywords as member names (e.g., OS.exec, thread.sleep)
            std::string member;
            if (check(TokenType::IDENTIFIER)) {
                member = advance().lexeme;
            } else {
                // Allow any keyword token as a member name after '.'
                Token tok = peek();
                if (tok.type != TokenType::TOKEN_EOF &&
                    tok.type != TokenType::LEFT_PAREN &&
                    tok.type != TokenType::DOT &&
                    tok.type != TokenType::SEMICOLON) {
                    member = advance().lexeme;
                } else {
                    error(tok, "Expected property name after '.'");
                    member = "?";
                }
            }
            auto node = std::make_shared<MemberAccessNode>();
            node->object = expr;
            node->member = member;
            node->line = previous().line;
            expr = node;
        } else if (match(TokenType::LEFT_BRACKET)) {
            // Index access
            ASTNodePtr index = expression();
            consume(TokenType::RIGHT_BRACKET, "Expected ']'");
            auto node = std::make_shared<IndexAccessNode>();
            node->object = expr;
            node->index = index;
            node->line = previous().line;
            expr = node;
        } else {
            break;
        }
    }

    return expr;
}

ASTNodePtr Parser::primary() {
    // Integer literal
    if (match(TokenType::INT_LITERAL)) {
        auto node = std::make_shared<LiteralNode>();
        node->litType = LiteralNode::INT_LIT;
        node->intVal = previous().intValue;
        node->line = previous().line;
        return node;
    }

    // Float literal
    if (match(TokenType::FLOAT_LITERAL)) {
        auto node = std::make_shared<LiteralNode>();
        node->litType = LiteralNode::FLOAT_LIT;
        node->floatVal = previous().floatValue;
        node->line = previous().line;
        return node;
    }

    // String literal
    if (match(TokenType::STRING_LITERAL)) {
        auto node = std::make_shared<LiteralNode>();
        node->litType = LiteralNode::STRING_LIT;
        node->stringVal = previous().stringValue;
        node->line = previous().line;
        return node;
    }

    // Char literal
    if (match(TokenType::CHAR_LITERAL)) {
        auto node = std::make_shared<LiteralNode>();
        node->litType = LiteralNode::CHAR_LIT;
        node->charVal = previous().charValue;
        node->line = previous().line;
        return node;
    }

    // Boolean literals
    if (match(TokenType::KW_TRUE)) {
        auto node = std::make_shared<LiteralNode>();
        node->litType = LiteralNode::BOOL_LIT;
        node->boolVal = true;
        node->line = previous().line;
        return node;
    }
    if (match(TokenType::KW_FALSE)) {
        auto node = std::make_shared<LiteralNode>();
        node->litType = LiteralNode::BOOL_LIT;
        node->boolVal = false;
        node->line = previous().line;
        return node;
    }

    // Null literal
    if (match(TokenType::KW_NULL)) {
        auto node = std::make_shared<LiteralNode>();
        node->litType = LiteralNode::NULL_LIT;
        node->line = previous().line;
        return node;
    }

    // 'new' expression: new ClassName(args)
    if (match(TokenType::KW_NEW)) {
        std::string className = consume(TokenType::IDENTIFIER, "Expected class name after 'new'").lexeme;
        consume(TokenType::LEFT_PAREN, "Expected '(' after class name in 'new' expression");

        auto node = std::make_shared<NewExprNode>();
        node->className = className;
        node->line = previous().line;

        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                // Check for named arguments: name: value
                if (check(TokenType::IDENTIFIER) && current + 1 < (int)tokens.size() &&
                    tokens[current + 1].type == TokenType::COLON) {
                    std::string argName = advance().lexeme;
                    advance(); // consume :
                    node->argNames.push_back(argName);
                    node->arguments.push_back(expression());
                } else {
                    node->argNames.push_back("");
                    node->arguments.push_back(expression());
                }
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_PAREN, "Expected ')' after constructor arguments");
        return node;
    }

    // List literal: [1, 2, 3]
    if (match(TokenType::LEFT_BRACKET)) {
        auto node = std::make_shared<ListLiteralNode>();
        node->line = previous().line;

        if (!check(TokenType::RIGHT_BRACKET)) {
            do {
                node->elements.push_back(expression());
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_BRACKET, "Expected ']'");
        return node;
    }

    // Type keyword used as value (e.g., int.random, float.random)
    if (isTypeKeyword(peek().type)) {
        Token typeToken = advance();
        if (check(TokenType::DOT)) {
            // Type member access: int.random, float.random, etc.
            auto varNode = std::make_shared<VariableNode>(typeToken.lexeme);
            varNode->line = typeToken.line;
            return varNode;
        }
        // Constructors for vec2, vec3, etc.: vec2(1.0, 2.0)
        if (check(TokenType::LEFT_PAREN)) {
            auto varNode = std::make_shared<VariableNode>(typeToken.lexeme);
            varNode->line = typeToken.line;
            return varNode;
        }
        // Just a type name used as expression (shouldn't normally happen)
        auto varNode = std::make_shared<VariableNode>(typeToken.lexeme);
        varNode->line = typeToken.line;
        return varNode;
    }

    // 'super' keyword used as expression (for super.init(...) calls)
    if (match(TokenType::KW_SUPER)) {
        auto node = std::make_shared<VariableNode>("super");
        node->line = previous().line;
        return node;
    }

    // Contextual keywords that can be used as identifiers in expressions
    // (e.g., thread.sleep(), OS.exec(), mutex.lock())
    if (match(TokenType::KW_THREAD) || match(TokenType::KW_EXEC) ||
        match(TokenType::KW_MUTEX) || match(TokenType::KW_ATOMIC)) {
        auto node = std::make_shared<VariableNode>(previous().lexeme);
        node->line = previous().line;
        return node;
    }

    // Identifier
    if (match(TokenType::IDENTIFIER)) {
        auto node = std::make_shared<VariableNode>(previous().lexeme);
        node->line = previous().line;
        return node;
    }

    // Grouped expression or lambda
    if (check(TokenType::LEFT_PAREN)) {
        // Check if this is a lambda: (...) => ...
        if (checkLambda()) {
            return parseLambda();
        }

        advance(); // consume (
        ASTNodePtr expr = expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')'");
        return expr;
    }

    // Struct literal: { field: value, field: value, ... }
    if (check(TokenType::LEFT_BRACE) &&
        current + 1 < (int)tokens.size() && tokens[current + 1].type == TokenType::IDENTIFIER &&
        current + 2 < (int)tokens.size() && tokens[current + 2].type == TokenType::COLON) {
        advance(); // consume {
        auto node = std::make_shared<StructInitNode>();
        node->line = previous().line;
        do {
            std::string fieldName = consume(TokenType::IDENTIFIER, "Expected field name").lexeme;
            consume(TokenType::COLON, "Expected ':' after field name");
            ASTNodePtr value = expression();
            node->fields.push_back({fieldName, value});
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_BRACE, "Expected '}' after struct literal");
        return node;
    }

    error(peek(), "Expected expression");
    throw std::runtime_error(errors.back());
}

// ============================================================================
// Helper parsers
// ============================================================================

std::vector<Parameter> Parser::parseParameters() {
    std::vector<Parameter> params;

    if (check(TokenType::RIGHT_PAREN)) return params;

    do {
        Parameter param;
        param.typeName = parseTypeName();
        param.name = consume(TokenType::IDENTIFIER, "Expected parameter name").lexeme;

        // Default value: type name = value
        if (match(TokenType::EQUAL)) {
            param.defaultValue = expression();
        }

        params.push_back(param);
    } while (match(TokenType::COMMA));

    return params;
}

std::vector<ASTNodePtr> Parser::parseArguments() {
    std::vector<ASTNodePtr> args;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            args.push_back(expression());
        } while (match(TokenType::COMMA));
    }
    return args;
}

ASTNodePtr Parser::finishCall(ASTNodePtr callee) {
    auto node = std::make_shared<CallNode>();
    node->callee = callee;
    node->line = previous().line;

    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            // Check for named arguments: name: value
            if (check(TokenType::IDENTIFIER) && current + 1 < (int)tokens.size() &&
                tokens[current + 1].type == TokenType::COLON) {
                std::string argName = advance().lexeme;
                advance(); // consume :
                node->argNames.push_back(argName);
                node->arguments.push_back(expression());
            } else {
                node->argNames.push_back("");
                node->arguments.push_back(expression());
            }
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
    return node;
}

bool Parser::checkLambda() {
    // Look ahead: if we find (...) => then it's a lambda
    if (!check(TokenType::LEFT_PAREN)) return false;

    int depth = 0;
    int pos = current;
    while (pos < (int)tokens.size()) {
        if (tokens[pos].type == TokenType::LEFT_PAREN) depth++;
        else if (tokens[pos].type == TokenType::RIGHT_PAREN) {
            depth--;
            if (depth == 0) {
                // Check if next token after ) is =>
                if (pos + 1 < (int)tokens.size() &&
                    tokens[pos + 1].type == TokenType::FAT_ARROW) {
                    return true;
                }
                return false;
            }
        }
        pos++;
    }
    return false;
}

ASTNodePtr Parser::parseLambda() {
    consume(TokenType::LEFT_PAREN, "Expected '(' for lambda");

    auto node = std::make_shared<LambdaNode>();
    node->line = previous().line;

    // Parse params: can be typed (int x, int y) or untyped (a, b)
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            Parameter param;
            // Check if next-next is an identifier (typed param) or just an identifier (untyped)
            if (isTypeToken() && current + 1 < (int)tokens.size() &&
                (tokens[current + 1].type == TokenType::IDENTIFIER ||
                 tokens[current + 1].type == TokenType::STAR ||
                 tokens[current + 1].type == TokenType::LEFT_BRACKET)) {
                param.typeName = parseTypeName();
                param.name = consume(TokenType::IDENTIFIER, "Expected parameter name").lexeme;
            } else if (check(TokenType::IDENTIFIER)) {
                param.typeName = "";
                param.name = advance().lexeme;
            }
            node->params.push_back(param);
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after lambda parameters");
    consume(TokenType::FAT_ARROW, "Expected '=>' after lambda parameters");

    // Body: either a block { ... } or a single expression
    if (check(TokenType::LEFT_BRACE)) {
        node->body = blockStatement();
    } else {
        // Single expression - wrap in a return statement
        auto ret = std::make_shared<ReturnStmtNode>();
        ret->value = expression();
        ret->line = previous().line;

        auto block = std::make_shared<BlockNode>();
        block->statements.push_back(ret);
        node->body = block;
    }

    return node;
}
