#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>

enum class SymbolType {
    VARIABLE,
    FUNCTION
};

enum class DataType {
    INT,
    DOUBLE,
    FLOAT,
    STRING,
    CHAR,
    VOID
};

struct Symbol {
    std::string name;
    SymbolType type;
    DataType dataType;
    size_t size;

// Domyślny konstruktor
    Symbol() : name(""), type(SymbolType::VARIABLE), dataType(DataType::INT), size(0) {}

    // Konstruktor z czterema argumentami
    Symbol(const std::string& name, SymbolType type, DataType dataType, size_t dataSize)
        : name(name), type(type), dataType(dataType), size(dataSize) {}

    // Konstruktor z trzema argumentami (przykład z poprzednich wersji)
    Symbol(SymbolType type, DataType dataType, size_t dataSize) 
        : type(type), dataType(dataType), size(dataSize) {}
};

#endif // SYMBOL_H
