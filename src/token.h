#pragma once

#include <string>
#include <unordered_map>

// ============================================================================
// Token Types for the Flux Programming Language
// ============================================================================

enum class TokenType {
    // Single-character tokens
    LEFT_PAREN, RIGHT_PAREN,       // ( )
    LEFT_BRACE, RIGHT_BRACE,       // { }
    LEFT_BRACKET, RIGHT_BRACKET,   // [ ]
    COMMA, DOT, SEMICOLON, COLON,  // , . ; :

    // One or two character tokens
    MINUS, MINUS_MINUS, MINUS_EQUAL,        // - -- -=
    PLUS, PLUS_PLUS, PLUS_EQUAL,            // + ++ +=
    STAR, STAR_EQUAL,                        // * *=
    SLASH, SLASH_EQUAL,                      // / /=
    PERCENT, PERCENT_EQUAL,                  // % %=
    BANG, BANG_EQUAL,                         // ! !=
    EQUAL, EQUAL_EQUAL,                      // = ==
    LESS, LESS_EQUAL,                        // < <=
    GREATER, GREATER_EQUAL,                  // > >=
    ARROW,                                   // ->
    FAT_ARROW,                               // =>
    AMP_AMP,                                 // &&
    PIPE_PIPE,                               // ||

    // Bitwise operators
    AMPERSAND,                               // &
    PIPE,                                    // |
    CARET,                                   // ^
    TILDE,                                   // ~
    LEFT_SHIFT,                              // <<
    RIGHT_SHIFT,                             // >>
    AMP_EQUAL,                               // &=
    PIPE_EQUAL,                              // |=
    CARET_EQUAL,                             // ^=
    LEFT_SHIFT_EQUAL,                        // <<=
    RIGHT_SHIFT_EQUAL,                       // >>=

    // Ternary
    QUESTION,                                // ?

    // Special multi-character operators
    EQUAL_NUM_EQUAL,    // =num=
    EQUAL_WORD_EQUAL,   // =word=

    // Literals
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,

    // Type keywords
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_BYTE,
    TYPE_INT,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_VEC2,
    TYPE_VEC3,
    TYPE_MAT4,
    TYPE_COLOR32,

    // Value keywords
    KW_TRUE,
    KW_FALSE,
    KW_NULL,

    // Control flow keywords
    KW_IF,
    KW_ELIF,
    KW_ELSE,
    KW_FOR,
    KW_IN,
    KW_WHILE,
    KW_DO,
    KW_SWITCH,
    KW_CASE,
    KW_DEFAULT,
    KW_BREAK,
    KW_CONTINUE,
    KW_RETURN,

    // Function / class keywords
    KW_FUNC,
    KW_CLASS,
    KW_STRUCT,
    KW_ENUM,
    KW_INTERFACE,
    KW_EXTENDS,
    KW_IMPLEMENTS,
    KW_NEW,
    KW_SUPER,
    KW_PUBLIC,
    KW_PRIVATE,
    KW_PROTECTED,
    KW_CONST,

    // Module keywords
    KW_IMPORT,
    KW_EXPORT,

    // Error handling keywords
    KW_TRY,
    KW_CATCH,
    KW_THROW,
    KW_FINALLY,
    KW_PANIC,

    // Special keywords
    KW_UNSAFE,
    KW_CLEANUP,
    KW_ATOMIC,
    KW_MUTEX,
    KW_THREAD,
    KW_BUTNOT,
    KW_EXEC,
    KW_STATIC,
    KW_ASM,
    KW_OBJECT,

    // Identifier
    IDENTIFIER,

    // End of file
    TOKEN_EOF
};

// ============================================================================
// Token structure
// ============================================================================

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    // Literal values (used for parsed literal tokens)
    int64_t intValue = 0;
    double floatValue = 0.0;
    std::string stringValue;
    char charValue = '\0';

    Token() : type(TokenType::TOKEN_EOF), line(0), column(0) {}

    Token(TokenType type, const std::string& lexeme, int line, int column)
        : type(type), lexeme(lexeme), line(line), column(column) {}
};

// ============================================================================
// Keyword lookup table
// ============================================================================

inline const std::unordered_map<std::string, TokenType>& getKeywords() {
    static const std::unordered_map<std::string, TokenType> keywords = {
        // Type keywords
        {"void",    TokenType::TYPE_VOID},
        {"bool",    TokenType::TYPE_BOOL},
        {"char",    TokenType::TYPE_CHAR},
        {"byte",    TokenType::TYPE_BYTE},
        {"int",     TokenType::TYPE_INT},
        {"long",    TokenType::TYPE_LONG},
        {"float",   TokenType::TYPE_FLOAT},
        {"string",  TokenType::TYPE_STRING},
        {"vec2",    TokenType::TYPE_VEC2},
        {"vec3",    TokenType::TYPE_VEC3},
        {"mat4",    TokenType::TYPE_MAT4},
        {"color32", TokenType::TYPE_COLOR32},

        // Value keywords
        {"true",    TokenType::KW_TRUE},
        {"false",   TokenType::KW_FALSE},
        {"null",    TokenType::KW_NULL},

        // Control flow
        {"if",       TokenType::KW_IF},
        {"elif",     TokenType::KW_ELIF},
        {"else",     TokenType::KW_ELSE},
        {"for",      TokenType::KW_FOR},
        {"in",       TokenType::KW_IN},
        {"while",    TokenType::KW_WHILE},
        {"do",       TokenType::KW_DO},
        {"switch",   TokenType::KW_SWITCH},
        {"case",     TokenType::KW_CASE},
        {"default",  TokenType::KW_DEFAULT},
        {"break",    TokenType::KW_BREAK},
        {"continue", TokenType::KW_CONTINUE},
        {"return",   TokenType::KW_RETURN},

        // Function / class
        {"func",       TokenType::KW_FUNC},
        {"class",      TokenType::KW_CLASS},
        {"struct",     TokenType::KW_STRUCT},
        {"enum",       TokenType::KW_ENUM},
        {"interface",  TokenType::KW_INTERFACE},
        {"extends",    TokenType::KW_EXTENDS},
        {"implements", TokenType::KW_IMPLEMENTS},
        {"new",        TokenType::KW_NEW},
        {"super",      TokenType::KW_SUPER},
        {"public",     TokenType::KW_PUBLIC},
        {"private",    TokenType::KW_PRIVATE},
        {"protected",  TokenType::KW_PROTECTED},
        {"const",      TokenType::KW_CONST},

        // Module
        {"import",  TokenType::KW_IMPORT},
        {"export",  TokenType::KW_EXPORT},

        // Error handling
        {"try",     TokenType::KW_TRY},
        {"catch",   TokenType::KW_CATCH},
        {"throw",   TokenType::KW_THROW},
        {"finally", TokenType::KW_FINALLY},
        {"panic",   TokenType::KW_PANIC},

        // Special
        {"unsafe",  TokenType::KW_UNSAFE},
        {"cleanup", TokenType::KW_CLEANUP},
        {"atomic",  TokenType::KW_ATOMIC},
        {"mutex",   TokenType::KW_MUTEX},
        {"thread",  TokenType::KW_THREAD},
        {"butnot",  TokenType::KW_BUTNOT},
        {"exec",    TokenType::KW_EXEC},
        {"static",  TokenType::KW_STATIC},
        {"asm",     TokenType::KW_ASM},
        {"object",  TokenType::KW_OBJECT},
    };
    return keywords;
}

// Helper: check if a token type is a type keyword
inline bool isTypeKeyword(TokenType t) {
    return t == TokenType::TYPE_VOID || t == TokenType::TYPE_BOOL ||
           t == TokenType::TYPE_CHAR || t == TokenType::TYPE_BYTE ||
           t == TokenType::TYPE_INT  || t == TokenType::TYPE_LONG ||
           t == TokenType::TYPE_FLOAT || t == TokenType::TYPE_STRING ||
           t == TokenType::TYPE_VEC2 || t == TokenType::TYPE_VEC3 ||
           t == TokenType::TYPE_MAT4 || t == TokenType::TYPE_COLOR32 ||
           t == TokenType::KW_OBJECT;
}

// Helper: get human-readable token name
inline std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::LEFT_PAREN:     return "(";
        case TokenType::RIGHT_PAREN:    return ")";
        case TokenType::LEFT_BRACE:     return "{";
        case TokenType::RIGHT_BRACE:    return "}";
        case TokenType::LEFT_BRACKET:   return "[";
        case TokenType::RIGHT_BRACKET:  return "]";
        case TokenType::COMMA:          return ",";
        case TokenType::DOT:            return ".";
        case TokenType::SEMICOLON:      return ";";
        case TokenType::COLON:          return ":";
        case TokenType::EQUAL:          return "=";
        case TokenType::EQUAL_EQUAL:    return "==";
        case TokenType::BANG_EQUAL:     return "!=";
        case TokenType::ARROW:          return "->";
        case TokenType::FAT_ARROW:      return "=>";
        case TokenType::EQUAL_NUM_EQUAL:  return "=num=";
        case TokenType::EQUAL_WORD_EQUAL: return "=word=";
        case TokenType::TOKEN_EOF:      return "EOF";
        case TokenType::IDENTIFIER:     return "IDENTIFIER";
        case TokenType::INT_LITERAL:    return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL:  return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        default: return "TOKEN";
    }
}
