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
    UNKNOWN
};

// Dodaj konstruktor do struktury Token
struct Token {
    TokenType type;
    std::string value;

    // Konstruktor zainicjalizujący pola typu i wartości
    Token(TokenType t, const std::string& v) : type(t), value(v) {}
};

// Dodaj definicję operatora << dla struktury Token
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
};

#endif // LEXER_H
