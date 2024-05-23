#include "lexer.h"
#include "debug.h"

const std::unordered_map<std::string, TokenType> keywords = {
    {"auto", TokenType::KEYWORD}, {"break", TokenType::KEYWORD}, {"case", TokenType::KEYWORD},
    {"char", TokenType::KEYWORD}, {"const", TokenType::KEYWORD}, {"continue", TokenType::KEYWORD},
    {"default", TokenType::KEYWORD}, {"do", TokenType::KEYWORD}, {"double", TokenType::KEYWORD},
    {"else", TokenType::KEYWORD}, {"enum", TokenType::KEYWORD}, {"extern", TokenType::KEYWORD},
    {"float", TokenType::KEYWORD}, {"for", TokenType::KEYWORD}, {"goto", TokenType::KEYWORD},
    {"if", TokenType::KEYWORD}, {"int", TokenType::KEYWORD}, {"long", TokenType::KEYWORD},
    {"register", TokenType::KEYWORD}, {"return", TokenType::KEYWORD}, {"short", TokenType::KEYWORD},
    {"signed", TokenType::KEYWORD}, {"sizeof", TokenType::KEYWORD}, {"static", TokenType::KEYWORD},
    {"struct", TokenType::KEYWORD}, {"switch", TokenType::KEYWORD}, {"typedef", TokenType::KEYWORD},
    {"union", TokenType::KEYWORD}, {"unsigned", TokenType::KEYWORD}, {"void", TokenType::KEYWORD},
    {"volatile", TokenType::KEYWORD}, {"while", TokenType::KEYWORD}, {"bool", TokenType::KEYWORD},
    {"class", TokenType::KEYWORD}, {"const_cast", TokenType::KEYWORD}, {"delete", TokenType::KEYWORD},
    {"dynamic_cast", TokenType::KEYWORD}, {"explicit", TokenType::KEYWORD}, {"export", TokenType::KEYWORD},
    {"false", TokenType::KEYWORD}, {"friend", TokenType::KEYWORD}, {"inline", TokenType::KEYWORD},
    {"mutable", TokenType::KEYWORD}, {"namespace", TokenType::KEYWORD}, {"new", TokenType::KEYWORD},
    {"operator", TokenType::KEYWORD}, {"private", TokenType::KEYWORD}, {"protected", TokenType::KEYWORD},
    {"public", TokenType::KEYWORD}, {"reinterpret_cast", TokenType::KEYWORD}, {"static_cast", TokenType::KEYWORD},
    {"template", TokenType::KEYWORD}, {"this", TokenType::KEYWORD}, {"throw", TokenType::KEYWORD},
    {"true", TokenType::KEYWORD}, {"try", TokenType::KEYWORD}, {"typeid", TokenType::KEYWORD},
    {"typename", TokenType::KEYWORD}, {"using", TokenType::KEYWORD}, {"virtual", TokenType::KEYWORD},
    {"wchar_t", TokenType::KEYWORD}, {"and", TokenType::KEYWORD}, {"and_eq", TokenType::KEYWORD},
    {"asm", TokenType::KEYWORD}, {"bitand", TokenType::KEYWORD}, {"bitor", TokenType::KEYWORD},
    {"compl", TokenType::KEYWORD}, {"not", TokenType::KEYWORD}, {"not_eq", TokenType::KEYWORD},
    {"or", TokenType::KEYWORD}, {"or_eq", TokenType::KEYWORD}, {"xor", TokenType::KEYWORD},
    {"xor_eq", TokenType::KEYWORD}
};

const std::vector<TokenPattern> tokenPatterns = {
    {TokenType::IDENTIFIER, std::regex(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\b)")},
    {TokenType::NUMBER, std::regex(R"(\b\d+(\.\d+)?\b)")},
    {TokenType::STRING, std::regex(R"("([^"\\]|\\.)*")")},
    {TokenType::CHAR, std::regex(R"('([^'\\]|\\.)*')")},
    {TokenType::OPERATOR, std::regex(R"([+\-*/=<>!&|]{1,2})")},
    {TokenType::SEPARATOR, std::regex(R"([;,\(\)\{\}])")}
};



Lexer::Lexer(const std::string& source) : sourceCode(source), position(0){}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (position < sourceCode.length()) {
        if (std::isspace(sourceCode[position])) {
            ++position;
            continue;
        }
        Token token = getNextToken();
        if (token.type != TokenType::UNKNOWN && token.type != TokenType::WHITESPACE) {  // Ignore whitespace tokens
            tokens.push_back(token);
        }
    }
    return tokens;
}


Token Lexer::getNextToken() {
    std::string remainingCode = sourceCode.substr(position);
    std::cout << "Remaining code: " << remainingCode << std::endl; // Debug print


    // Sprawdź, czy pasuje do słowa kluczowego
    std::smatch match;
    if (std::regex_search(remainingCode, match, std::regex(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\b)")) && match.position() == 0) {
        std::string identifier = match.str(0);
        position += identifier.length();
        trackDebugInfo(sourceCode, position, lineNumber, columnNumber);
        auto it = keywords.find(identifier);
        if (it != keywords.end()) {
            return {it->second, identifier};
        }
        return {TokenType::IDENTIFIER, identifier};
    }

    // Sprawdź pozostałe wzorce
    for (const auto& pattern : tokenPatterns) {
        if (std::regex_search(remainingCode, match, pattern.pattern) && match.position() == 0) {
            position += match.length();
            std::string tokenValue = match.str();
            // Usuń zewnętrzne cudzysłowy dla STRING i CHAR
            if (pattern.type == TokenType::STRING || pattern.type == TokenType::CHAR) {
                tokenValue = tokenValue.substr(1, tokenValue.length() - 2);
            }
            trackDebugInfo(sourceCode, position, lineNumber, columnNumber);
            return {pattern.type, tokenValue};
        }
    }

    return {TokenType::UNKNOWN, std::string(1, sourceCode[position])};
}
