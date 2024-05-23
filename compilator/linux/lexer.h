#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include <iostream>

enum class TokenType {
    KEYWORD,
    IDENTIFIER,
    NUMBER,
    STRING,
    CHAR,
    OPERATOR,
    SEPARATOR,
    UNKNOWN,
    WHITESPACE  // Added WHITESPACE token type
};

struct Token {
    TokenType type;
    std::string value;

    Token(TokenType t, const std::string& v) : type(t), value(v) {}
};

inline std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "Token(";
    switch (token.type) {
        case TokenType::KEYWORD: os << "KEYWORD"; break;
        case TokenType::IDENTIFIER: os << "IDENTIFIER"; break;
        case TokenType::NUMBER: os << "NUMBER"; break;
        case TokenType::STRING: os << "STRING"; break;
        case TokenType::CHAR: os << "CHAR"; break;
        case TokenType::OPERATOR: os << "OPERATOR"; break;
        case TokenType::SEPARATOR: os << "SEPARATOR"; break;
        case TokenType::WHITESPACE: os << "WHITESPACE"; break;  // Added WHITESPACE
        default: os << "UNKNOWN"; break;
    }
    os << ", \"" << token.value << "\")";
    return os;
}

struct TokenPattern {
    TokenType type;
    std::regex pattern;
};

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    Token getNextToken();
    std::string sourceCode;
    size_t position;
    size_t lineNumber;    // Added line number
    size_t columnNumber;  // Added column number
};

#endif // LEXER_H
