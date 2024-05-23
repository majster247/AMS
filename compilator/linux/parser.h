#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include "lexer.h"
#include "symbol_table.h"

enum class NodeType {
    PROGRAM,
    STATEMENT,
    EXPRESSION,
    VARIABLE_DECLARATION,
    ASSIGNMENT,
    IF_STATEMENT,
    ELSE_STATEMENT,
    RETURN_STATEMENT
};

struct Node {
    NodeType type;
    std::vector<Node> children;
    Token token;

    // Konstruktor dla węzła bez dzieci
    Node(NodeType type) : type(type), token(Token{TokenType::UNKNOWN, ""}) {}

    // Konstruktor dla węzła z jednym dzieckiem
    Node(NodeType type, const Node& child) : type(type), token(Token{TokenType::UNKNOWN, ""}) {
        children.push_back(child);
    }

    // Konstruktor dla węzła z tokenem i bez dzieci
    Node(NodeType type, const Token& token) : type(type), token(token) {}

    // Konstruktor dla węzła z tokenem i jednym dzieckiem
    Node(NodeType type, const Token& token, const Node& child) : type(type), token(token) {
        children.push_back(child);
    }
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens, SymbolTable& symbolTable);
    Node parse();



private:
    Node parseStatement();
    Node parseExpression();
    Node parseSimpleExpression();
    Node parseVariableDeclaration();
    Node parseAssignment();
    Node parseIfStatement();
    Node parseReturnStatement();

    const std::vector<Token>& tokens;
    SymbolTable& symbolTable;
    size_t position;

};

#endif // PARSER_H
