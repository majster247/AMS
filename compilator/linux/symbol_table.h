#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <unordered_map>
#include <stdexcept> // For exception handling
#include <iostream>
#include "symbol.h"


class SymbolTable {
public:
    SymbolTable();

    void addSymbol(const std::string& name, SymbolType type, DataType dataType, size_t dataSize = 0);
    Symbol getSymbol(const std::string& name) const;
    void updateSymbolSize(const std::string& name, size_t newSize);
    bool symbolExists(const std::string& name);
    void displaySymbolTable(const SymbolTable& symbolTable);
    DataType getSymbolDataType(const std::string& name) const;

private:
    std::unordered_map<std::string, Symbol> symbols;
};
std::ostream& operator<<(std::ostream& os, const SymbolType& type);
std::ostream& operator<<(std::ostream& os, const DataType& type);


#endif // SYMBOL_TABLE_H
