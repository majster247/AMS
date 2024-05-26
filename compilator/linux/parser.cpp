#include "parser.h"
#include "debug.h"
#include <stdexcept>

#define DEFAULT_STRING_SIZE 256

Parser::Parser(const std::vector<Token>& tokens, SymbolTable& symbolTable) : tokens(tokens), symbolTable(symbolTable), position(0) {}

Node Parser::parse() {
    Node programNode(NodeType::PROGRAM);
    while (position < tokens.size()) {
        programNode.children.push_back(parseStatement());
    }
    return programNode;
}

Node Parser::parseStatement() {
    const Token& token = tokens[position];
    trackDebugInfo(tokens[position].value, position, lineNumber, columnNumber);

    if (token.type == TokenType::KEYWORD && token.value == "int") {
        return parseVariableDeclaration();
    } else if (token.type == TokenType::IDENTIFIER) {
        return parseAssignment();
    } else if (token.type == TokenType::KEYWORD && token.value == "if") {
        return parseIfStatement();
    } else if (token.type == TokenType::KEYWORD && token.value == "return") {
        return parseReturnStatement();
    }

    throw std::runtime_error("Unexpected token: " + token.value);
}

Node Parser::parseExpression() {
    Node left = parseSimpleExpression();
    
    while (position < tokens.size() && tokens[position].type == TokenType::OPERATOR && tokens[position].value == "==") {
        Token op = tokens[position++];
        Node right = parseSimpleExpression();
        Node node(NodeType::EXPRESSION, op);
        node.children.push_back(left);
        node.children.push_back(right);
        left = node;
    }
    
    return left;
}

Node Parser::parseSimpleExpression() {
    const Token& token = tokens[position];
    if (token.type == TokenType::NUMBER || token.type == TokenType::IDENTIFIER) {
        Node node(NodeType::EXPRESSION, token);
        position++;
        return node;
    }
    throw std::runtime_error("Unexpected token in expression: " + token.value);
}

Node Parser::parseVariableDeclaration() {
    if (position + 1 >= tokens.size()) {
        throw std::runtime_error("Unexpected end of input");
    }

    std::string dataType = tokens[position].value;
    std::string varName = tokens[position + 1].value;
    position += 2;

    if (symbolTable.symbolExists(varName)) {
        throw std::runtime_error("Variable already declared: " + varName);
    }

    trackDebugInfo(tokens[position].value, position, lineNumber, columnNumber);

    size_t dataSize;
    if (dataType == "int") {
        dataSize = sizeof(int);
    } else if (dataType == "float") {
        dataSize = sizeof(float);
    } else if (dataType == "char") {
        dataSize = sizeof(char);
    } else if (dataType == "double") {
        dataSize = sizeof(double);
    } else if (dataType == "string") {
        dataSize = DEFAULT_STRING_SIZE;
    } else {
        throw std::runtime_error("Unknown data type: " + dataType);
    }

    DataType dt = dataType == "int" ? DataType::INT :
                  dataType == "float" ? DataType::FLOAT :
                  dataType == "char" ? DataType::CHAR :
                  dataType == "double" ? DataType::DOUBLE :
                  dataType == "string" ? DataType::STRING : DataType::INT;
    symbolTable.addSymbol(varName, SymbolType::VARIABLE, dt, dataSize);

    Node node(NodeType::VARIABLE_DECLARATION, tokens[position - 2]);
    node.children.push_back(Node(NodeType::EXPRESSION, Token(TokenType::IDENTIFIER, dataType)));
    node.children.push_back(Node(NodeType::EXPRESSION, Token(TokenType::IDENTIFIER, varName)));

    if (position < tokens.size() && tokens[position].type == TokenType::OPERATOR && tokens[position].value == "=") {
        position++;
        node.children.push_back(parseExpression());
    }

    // Sprawdź czy po deklaracji zmiennej jest średnik
    if (position < tokens.size() && tokens[position].type == TokenType::SEPARATOR && tokens[position].value == ";") {
        position++;
    } else {
        // Increment column number by the length of the token value and one for the assignment operator
        columnNumber += tokens[position].value.length() + 1;
        throw std::runtime_error("Expected ';' after variable declaration. Line: " + std::to_string(lineNumber) + ", Column: " + std::to_string(columnNumber)); //error is here, need to check why
    }

    return node;
}





Node Parser::parseAssignment() {
    std::string varName = tokens[position].value;
    position++; // Skip variable name
    if (tokens[position].type != TokenType::OPERATOR || tokens[position].value != "=") {
        throw std::runtime_error("Expected '=' in assignment.");
    }
    position++; // Skip '='

    Node exprNode = parseExpression();
    size_t dataSize = 0;
    if (symbolTable.getSymbolDataType(varName) == DataType::STRING) {
        dataSize = exprNode.token.value.length();
    }
    symbolTable.updateSymbolSize(varName, dataSize);

    Node node(NodeType::ASSIGNMENT, Token(TokenType::OPERATOR, "="));
    node.children.push_back(Node(NodeType::EXPRESSION, Token(TokenType::IDENTIFIER, varName)));
    node.children.push_back(exprNode);

    if (position < tokens.size() && tokens[position].type == TokenType::SEPARATOR && tokens[position].value == ";") {
        position++;
    } else {
        throw std::runtime_error("Expected ';' after assignment.");
    }

    return node;
}

Node Parser::parseIfStatement() {
    Token ifToken = tokens[position];
    position++; // Skip 'if'

    if (tokens[position].type != TokenType::SEPARATOR || tokens[position].value != "(") {
        throw std::runtime_error("Expected '(' after 'if'.");
    }
    position++; // Skip '('
    Node conditionNode = parseExpression();
    if (tokens[position].type != TokenType::SEPARATOR || tokens[position].value != ")") {
        throw std::runtime_error("Expected ')' after condition.");
    }
    position++; // Skip ')'

    if (tokens[position].type != TokenType::SEPARATOR || tokens[position].value != "{") {
        throw std::runtime_error("Expected '{' after 'if' condition.");
    }
    position++; // Skip '{'
    Node ifNode(NodeType::IF_STATEMENT, ifToken);
    ifNode.children.push_back(conditionNode);
    while (tokens[position].type != TokenType::SEPARATOR || tokens[position].value != "}") {
        ifNode.children.push_back(parseStatement());
    }
    position++; // Skip '}'

    if (position < tokens.size() && tokens[position].type == TokenType::KEYWORD && tokens[position].value == "else") {
        Token elseToken = tokens[position];
        position++; // Skip 'else'
        if (tokens[position].type != TokenType::SEPARATOR || tokens[position].value != "{") {
            throw std::runtime_error("Expected '{' after 'else'.");
        }
        position++; // Skip '{'
        Node elseNode(NodeType::ELSE_STATEMENT, elseToken);
        while (tokens[position].type != TokenType::SEPARATOR || tokens[position].value != "}") {
            elseNode.children.push_back(parseStatement());
            if (position == tokens.size()) {
                throw std::runtime_error("Unexpected end of input in else block.");
            }
        }
        position++; // Skip '}'
        ifNode.children.push_back(elseNode);

        // Sprawdź czy po bloku 'else' jest średnik
        if (position < tokens.size() && tokens[position].type == TokenType::SEPARATOR && tokens[position].value == ";") {
            position++;
        } else {
            throw std::runtime_error("Expected ';' after 'else' block.");
        }
    }

    return ifNode;
}



Node Parser::parseReturnStatement() {
    Token returnToken = tokens[position];
    position++; // Skip 'return'
    Node exprNode = parseExpression();
    Node returnNode(NodeType::RETURN_STATEMENT, returnToken);
    returnNode.children.push_back(exprNode);

    if (position < tokens.size() && tokens[position].type == TokenType::SEPARATOR && tokens[position].value == ";") {
        position++;
    } else {
        throw std::runtime_error("Expected ';' after return statement.");
    }

    return returnNode;
}



































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































//XD