#include "parser.h"

Parser::Parser(const std::vector<Token>& tokens, SymbolTable& symbolTable) : tokens(tokens), symbolTable(symbolTable), position(0) {}

Node Parser::parse() {
    Node programNode(NodeType::PROGRAM);
    while (position < tokens.size()) {
        programNode.children.push_back(parseStatement());
    }
    return programNode;
}

Node Parser::parseStatement() {
    if (tokens[position].type == TokenType::KEYWORD) {
        if (tokens[position].value == "if") {
            return parseIfStatement();
        } else if (tokens[position].value == "return") {
            return parseReturnStatement();
        }
    }
    return parseExpression();
}

Node Parser::parseExpression() {
    // Na początek obsłużmy proste przypisania i deklaracje zmiennych
    if (tokens[position].type == TokenType::IDENTIFIER) {
        if (position + 1 < tokens.size() && tokens[position + 1].type == TokenType::OPERATOR && tokens[position + 1].value == "=") {
            return parseAssignment();
        }
    }

    // Sprawdź, czy następny token to słowo kluczowe "int" oznaczające deklarację zmiennej
    if (tokens[position].type == TokenType::KEYWORD && tokens[position].value == "int") {
        return parseVariableDeclaration();
    }

    // Jeśli żaden z powyższych przypadków nie pasuje, utwórz węzeł dla wyrażenia
    Node node{NodeType::EXPRESSION, {}, tokens[position]};
    position++;
    return node;
}

Node Parser::parseVariableDeclaration() {
    // Upewnij się, że mamy wystarczająco tokenów w wektorze
    if (position + 2 >= tokens.size()) {
        throw std::runtime_error("Unexpected end of input");
    }

    // Pobierz typ danych
    std::string dataType = tokens[position].value;

    // Sprawdź, czy typ danych jest obsługiwany
    DataType dataTypeEnum;
    if (dataType == "int") {
        dataTypeEnum = DataType::INT;
    } else if (dataType == "float") {
        dataTypeEnum = DataType::FLOAT;
    } else if (dataType == "char") {
        dataTypeEnum = DataType::CHAR;
    } else if (dataType == "string") {
        dataTypeEnum = DataType::STRING;
    } else {
        throw std::runtime_error("Unsupported data type: " + dataType);
    }

    // Pobierz nazwę zmiennej
    std::string varName = tokens[position + 1].value;
    position += 2; // Przesuń pozycję do następnego tokena po nazwie zmiennej

    // Sprawdź, czy zmienna została już zadeklarowana
    if (symbolTable.symbolExists(varName)) {
        throw std::runtime_error("Variable already declared: " + varName);
    }

    // Dynamiczne przydzielanie wielkości dla stringa
    int dataSize = 0;
    if (dataTypeEnum == DataType::STRING) {
        // Wczytaj wielkość ze specjalnej lokalizacji w kodzie (np. po "=")
        if (position < tokens.size() && tokens[position].type == TokenType::OPERATOR && tokens[position].value == "=") {
            position++; // Przejdź do kolejnego tokenu po "="
            if (position < tokens.size() && tokens[position].type == TokenType::STRING) {
                dataSize = tokens[position].value.length();
            } else {
                throw std::runtime_error("Expected string literal after '='");
            }
        } else {
            // Domyślna wielkość dla stringa, jeśli nie jest określona w kodzie
            dataSize = DEFAULT_STRING_SIZE;
        }
    }

    // Dodaj zmienną do tabeli symboli
    symbolTable.addSymbol(varName, SymbolType::VARIABLE, dataTypeEnum, dataSize);

    // Utwórz węzeł dla deklaracji zmiennej i dodaj dzieci (typ i nazwę zmiennej)
    Node node{NodeType::VARIABLE_DECLARATION, {}, tokens[position - 2]}; // Użyj poprzedniego tokenu (typu) jako głównego tokenu węzła
    node.children.push_back({NodeType::EXPRESSION, {}, Token(TokenType::IDENTIFIER, dataType)}); // Dodaj typ zmiennej jako dziecko
    node.children.push_back({NodeType::EXPRESSION, {}, Token(TokenType::IDENTIFIER, varName)}); // Dodaj nazwę zmiennej jako dziecko

    return node;
}





Node Parser::parseAssignment() {
    // Upewnij się, że mamy wystarczająco tokenów w wektorze
    if (position + 2 >= tokens.size()) {
        throw std::runtime_error("Unexpected end of input");
    }

    // Pobierz nazwę zmiennej
    std::string varName = tokens[position].value;
    position++; // Przesuń pozycję do następnego tokenu po nazwie zmiennej

    // Sprawdź, czy zmienna istnieje w tabeli symboli
    if (!symbolTable.symbolExists(varName)) {
        throw std::runtime_error("Variable not declared: " + varName);
    }

    // Pobierz symbol z tabeli symboli
    Symbol symbol = symbolTable.getSymbol(varName);

    // Sprawdź, czy typ zmiennej to string
    if (symbol.getDataType() == DataType::STRING) {
        // Dynamicznie przydziel wielkość dla zmiennej typu string
        int dataSize = 0;
        if (position < tokens.size() && tokens[position].type == TokenType::OPERATOR && tokens[position].value == "=") {
            position++; // Przejdź do kolejnego tokenu po "="
            if (position < tokens.size() && tokens[position].type == TokenType::STRING) {
                dataSize = tokens[position].value.length();
            } else {
                throw std::runtime_error("Expected string literal after '='");
            }
        } else {
            // Jeśli wielkość nie jest określona, użyj domyślnej wartości
            dataSize = DEFAULT_STRING_SIZE;
        }

        // Zaktualizuj wielkość zmiennej w tabeli symboli
        symbolTable.updateSymbolSize(varName, dataSize);
    }

    Node node{NodeType::ASSIGNMENT, {}, Token(TokenType::OPERATOR, "=")};
    node.children.push_back({NodeType::EXPRESSION, {}, Token(TokenType::IDENTIFIER, varName)});
    position++; // Przeskocz operator "="
    node.children.push_back(parseExpression());
    return node;
}


Node Parser::parseIfStatement() {
    Node node{NodeType::IF_STATEMENT, {}, tokens[position]};
    position++; // Przeskocz "if"
    position++; // Przeskocz "("
    node.children.push_back(parseExpression());
    position++; // Przeskocz ")"
    node.children.push_back(parseStatement());
    return node;
}

Node Parser::parseReturnStatement() {
    Node node{NodeType::RETURN_STATEMENT, {}, tokens[position]};
    position++; // Przeskocz "return"
    node.children.push_back(parseExpression());

    // Jeśli zwracany typ to string, ustaw odpowiednią wielkość
    if (node.children.back().token.type == TokenType::STRING) {
        Symbol symbol = symbolTable.getSymbol("return"); // Pobierz symbol dla "return"
        int dataSize = node.children.back().token.value.length();
        symbolTable.updateSymbolSize("return", dataSize); // Zaktualizuj wielkość zmiennej "return"
    }

    return node;
}

