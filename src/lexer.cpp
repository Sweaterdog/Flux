#include "lexer.h"
#include <iostream>
#include <sstream>

// ============================================================================
// Lexer Implementation
// ============================================================================

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source(source), filename(filename) {}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        start = current;
        tokenStartCol = column;
        scanToken();
    }
    tokens.push_back(Token(TokenType::TOKEN_EOF, "", line, column));
    return tokens;
}

// ============================================================================
// Core scanning
// ============================================================================

void Lexer::scanToken() {
    char c = advance();
    switch (c) {
        // Single character tokens
        case '(': addToken(TokenType::LEFT_PAREN); break;
        case ')': addToken(TokenType::RIGHT_PAREN); break;
        case '{': addToken(TokenType::LEFT_BRACE); break;
        case '}': addToken(TokenType::RIGHT_BRACE); break;
        case '[': addToken(TokenType::LEFT_BRACKET); break;
        case ']': addToken(TokenType::RIGHT_BRACKET); break;
        case ',': addToken(TokenType::COMMA); break;
        case '.': addToken(TokenType::DOT); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case ':': addToken(TokenType::COLON); break;

        // One or two character operators
        case '+':
            if (match('+')) addToken(TokenType::PLUS_PLUS);
            else if (match('=')) addToken(TokenType::PLUS_EQUAL);
            else addToken(TokenType::PLUS);
            break;
        case '-':
            if (match('-')) addToken(TokenType::MINUS_MINUS);
            else if (match('=')) addToken(TokenType::MINUS_EQUAL);
            else if (match('>')) addToken(TokenType::ARROW);
            else addToken(TokenType::MINUS);
            break;
        case '*':
            if (match('=')) addToken(TokenType::STAR_EQUAL);
            else addToken(TokenType::STAR);
            break;
        case '/':
            if (match('/')) {
                skipLineComment();
            } else if (match('*')) {
                skipBlockComment();
            } else if (match('=')) {
                addToken(TokenType::SLASH_EQUAL);
            } else {
                addToken(TokenType::SLASH);
            }
            break;
        case '%':
            if (match('=')) addToken(TokenType::PERCENT_EQUAL);
            else addToken(TokenType::PERCENT);
            break;
        case '!':
            if (match('=')) addToken(TokenType::BANG_EQUAL);
            else addToken(TokenType::BANG);
            break;
        case '=':
            // Check for =num= and =word=
            if (peek() == 'n' && current + 3 < (int)source.size() &&
                source[current] == 'n' && source[current+1] == 'u' &&
                source[current+2] == 'm' && source[current+3] == '=') {
                current += 4;
                column += 4;
                addToken(TokenType::EQUAL_NUM_EQUAL, "=num=");
            } else if (peek() == 'w' && current + 4 < (int)source.size() &&
                source[current] == 'w' && source[current+1] == 'o' &&
                source[current+2] == 'r' && source[current+3] == 'd' &&
                source[current+4] == '=') {
                current += 5;
                column += 5;
                addToken(TokenType::EQUAL_WORD_EQUAL, "=word=");
            } else if (match('=')) {
                addToken(TokenType::EQUAL_EQUAL);
            } else if (match('>')) {
                addToken(TokenType::FAT_ARROW);
            } else {
                addToken(TokenType::EQUAL);
            }
            break;
        case '<':
            if (match('<')) {
                if (match('=')) addToken(TokenType::LEFT_SHIFT_EQUAL);
                else addToken(TokenType::LEFT_SHIFT);
            }
            else if (match('=')) addToken(TokenType::LESS_EQUAL);
            else addToken(TokenType::LESS);
            break;
        case '>':
            if (match('>')) {
                if (match('=')) addToken(TokenType::RIGHT_SHIFT_EQUAL);
                else addToken(TokenType::RIGHT_SHIFT);
            }
            else if (match('=')) addToken(TokenType::GREATER_EQUAL);
            else addToken(TokenType::GREATER);
            break;
        case '&':
            if (match('&')) addToken(TokenType::AMP_AMP);
            else if (match('=')) addToken(TokenType::AMP_EQUAL);
            else addToken(TokenType::AMPERSAND);
            break;
        case '|':
            if (match('|')) addToken(TokenType::PIPE_PIPE);
            else if (match('=')) addToken(TokenType::PIPE_EQUAL);
            else addToken(TokenType::PIPE);
            break;
        case '^':
            if (match('=')) addToken(TokenType::CARET_EQUAL);
            else addToken(TokenType::CARET);
            break;
        case '~':
            addToken(TokenType::TILDE);
            break;
        case '?':
            addToken(TokenType::QUESTION);
            break;

        // Comments (hash style)
        case '#':
            skipLineComment();
            break;

        // String literal
        case '"':
            scanString();
            break;

        // Character literal
        case '\'':
            scanChar();
            break;

        // Whitespace
        case ' ':
        case '\r':
        case '\t':
            break;
        case '\n':
            line++;
            column = 1;
            break;

        default:
            if (isDigit(c)) {
                scanNumber();
            } else if (isAlpha(c)) {
                scanIdentifier();
            } else {
                error(std::string("Unexpected character '") + c + "'");
            }
            break;
    }
}

char Lexer::advance() {
    char c = source[current++];
    column++;
    return c;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= (int)source.size()) return '\0';
    return source[current + 1];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    current++;
    column++;
    return true;
}

bool Lexer::isAtEnd() const {
    return current >= (int)source.size();
}

// ============================================================================
// Token construction
// ============================================================================

void Lexer::addToken(TokenType type) {
    std::string lexeme = source.substr(start, current - start);
    Token token(type, lexeme, line, tokenStartCol);
    tokens.push_back(token);
}

void Lexer::addToken(TokenType type, const std::string& lexeme) {
    Token token(type, lexeme, line, tokenStartCol);
    tokens.push_back(token);
}

// ============================================================================
// Specific scanners
// ============================================================================

void Lexer::scanString() {
    std::string value;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') {
            line++;
            column = 0;
        }
        if (peek() == '\\') {
            advance(); // consume backslash
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case 'b': value += '\b'; break;
                case 'a': value += '\a'; break;
                case 'f': value += '\f'; break;
                case 'v': value += '\v'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '$': value += '$'; break;
                case '0': value += '\0'; break;
                default:
                    value += '\\';
                    value += escaped;
                    break;
            }
        } else {
            value += advance();
        }
    }

    if (isAtEnd()) {
        error("Unterminated string literal");
        return;
    }

    advance(); // closing "

    Token token(TokenType::STRING_LITERAL, source.substr(start, current - start), line, tokenStartCol);
    token.stringValue = value;
    tokens.push_back(token);
}

void Lexer::scanChar() {
    char value;
    if (peek() == '\\') {
        advance(); // backslash
        char escaped = advance();
        switch (escaped) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case 'b': value = '\b'; break;
            case 'a': value = '\a'; break;
            case 'f': value = '\f'; break;
            case 'v': value = '\v'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '0': value = '\0'; break;
            default: value = escaped; break;
        }
    } else {
        value = advance();
    }

    if (peek() != '\'') {
        error("Unterminated character literal");
        return;
    }
    advance(); // closing '

    Token token(TokenType::CHAR_LITERAL, source.substr(start, current - start), line, tokenStartCol);
    token.charValue = value;
    tokens.push_back(token);
}

void Lexer::scanNumber() {
    bool isFloat = false;
    bool isHex = false;
    bool isBin = false;
    bool isLong = false;

    // Check for hex (0x) or binary (0b)
    if (source[start] == '0' && !isAtEnd()) {
        if (peek() == 'x' || peek() == 'X') {
            advance(); // consume x
            isHex = true;
            while (!isAtEnd() && (isDigit(peek()) ||
                   (peek() >= 'a' && peek() <= 'f') ||
                   (peek() >= 'A' && peek() <= 'F'))) {
                advance();
            }
        } else if (peek() == 'b' || peek() == 'B') {
            advance(); // consume b
            isBin = true;
            while (!isAtEnd() && (peek() == '0' || peek() == '1')) {
                advance();
            }
        }
    }

    if (!isHex && !isBin) {
        // Regular decimal number
        while (!isAtEnd() && isDigit(peek())) {
            advance();
        }
        // Check for decimal point
        if (!isAtEnd() && peek() == '.' && isDigit(peekNext())) {
            isFloat = true;
            advance(); // consume .
            while (!isAtEnd() && isDigit(peek())) {
                advance();
            }
        }
        // Check for scientific notation
        if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
            isFloat = true;
            advance(); // consume e
            if (!isAtEnd() && (peek() == '+' || peek() == '-')) {
                advance();
            }
            while (!isAtEnd() && isDigit(peek())) {
                advance();
            }
        }
    }

    // Check for long suffix
    if (!isFloat && !isAtEnd() && (peek() == 'L' || peek() == 'l')) {
        isLong = true;
        advance();
    }

    std::string numStr = source.substr(start, current - start);

    if (isFloat) {
        Token token(TokenType::FLOAT_LITERAL, numStr, line, tokenStartCol);
        token.floatValue = std::stod(numStr);
        tokens.push_back(token);
    } else {
        Token token(TokenType::INT_LITERAL, numStr, line, tokenStartCol);
        if (isHex) {
            token.intValue = std::stoll(numStr, nullptr, 16);
        } else if (isBin) {
            token.intValue = std::stoll(numStr.substr(2), nullptr, 2);
        } else {
            token.intValue = std::stoll(numStr);
        }
        tokens.push_back(token);
    }
}

void Lexer::scanIdentifier() {
    while (!isAtEnd() && isAlphaNumeric(peek())) {
        advance();
    }

    std::string text = source.substr(start, current - start);

    // Check if it's a keyword
    const auto& keywords = getKeywords();
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        addToken(it->second, text);
    } else {
        addToken(TokenType::IDENTIFIER, text);
    }
}

void Lexer::skipLineComment() {
    // Check for special // DOCTYPE {AOT} directive - just skip like any comment
    // The transpiler will handle this by scanning raw source
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    int nesting = 1;
    while (!isAtEnd() && nesting > 0) {
        if (peek() == '/' && peekNext() == '*') {
            advance(); advance();
            nesting++;
        } else if (peek() == '*' && peekNext() == '/') {
            advance(); advance();
            nesting--;
        } else {
            if (peek() == '\n') {
                line++;
                column = 0;
            }
            advance();
        }
    }
    if (nesting > 0) {
        error("Unterminated block comment");
    }
}

// ============================================================================
// Helpers
// ============================================================================

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

void Lexer::error(const std::string& message) {
    std::ostringstream oss;
    oss << filename << ":" << line << ":" << tokenStartCol << ": error: " << message;
    errors.push_back(oss.str());
}
