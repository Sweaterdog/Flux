#pragma once

#include "token.h"
#include <string>
#include <vector>

// ============================================================================
// Flux Lexer - Tokenizes Flux source code
// ============================================================================

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename = "<stdin>");

    // Tokenize entire source, returning all tokens (including EOF)
    std::vector<Token> tokenize();

    // Get any errors that occurred during lexing
    const std::vector<std::string>& getErrors() const { return errors; }
    bool hasErrors() const { return !errors.empty(); }

private:
    std::string source;
    std::string filename;
    std::vector<Token> tokens;
    std::vector<std::string> errors;

    int start = 0;      // Start of current token
    int current = 0;    // Current position in source
    int line = 1;
    int column = 1;
    int tokenStartCol = 1;

    // Core scanning
    void scanToken();
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    bool isAtEnd() const;

    // Token construction
    void addToken(TokenType type);
    void addToken(TokenType type, const std::string& lexeme);

    // Specific scanners
    void scanString();
    void scanChar();
    void scanNumber();
    void scanIdentifier();
    void skipLineComment();
    void skipBlockComment();

    // Helpers
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    void error(const std::string& message);
};
