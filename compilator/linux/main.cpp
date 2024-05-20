#include <iostream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "symbol_table.h"

// Funkcja pomocnicza do rekurencyjnego wyświetlania drzewa składniowego
void printSyntaxTree(const Node& node, int indent = 0) {
    for (int i = 0; i < indent; ++i) {
        std::cout << "  ";
    }
    std::cout << node.token << std::endl;
    for (const auto& child : node.children) {
        printSyntaxTree(child, indent + 1);
    }
}

int main() {
    std::string sourceCode = R"(
        int main() {
            int x = 10;
            if (x == 10) {
                return x;
            } else {
                return 0;
            }
        }
    )";

    // Tokenizacja
    Lexer lexer(sourceCode);
    std::vector<Token> tokens = lexer.tokenize();

    // Wyświetlenie tokenów
    std::cout << "Tokens:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << token << std::endl;
    }

    // Tabela symboli
    SymbolTable symbolTable;

  // Parsowanie
    Parser parser(tokens, symbolTable);
    Node syntaxTree = parser.parse();

    // Wyświetlanie drzewa składniowego
    std::cout << "\nSyntax Tree:\n";
    printSyntaxTree(syntaxTree);

    // Wyświetlanie zawartości tabeli symboli
    symbolTable.displaySymbolTable(symbolTable);

    return 0;
}
