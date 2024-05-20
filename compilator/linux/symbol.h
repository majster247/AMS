#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>

enum class SymbolType {
    VARIABLE,
    FUNCTION
};

enum class DataType {
    INT,
    FLOAT,
    STRING,
    CHAR,
    VOID
};

struct Symbol {
    std::string name;
    SymbolType type;
    DataType dataType;
    int dataSize; // Dodatkowe pole dla rozmiaru danych
};

#endif // SYMBOL_H
