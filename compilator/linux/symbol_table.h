#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <unordered_map>
#include <stdexcept> // For exception handling
#include <iostream>
#include "symbol.h"


class SymbolTable {
public:
    void addSymbol(const std::string& name, SymbolType type, DataType dataType, int dataSize = 0);
    Symbol getSymbol(const std::string& name) const;
    bool symbolExists(const std::string& name);
    void displaySymbolTable(const SymbolTable& symbolTable); // Removed parameter, as it's not needed

private:
    std::unordered_map<std::string, Symbol> symbols;
};
std::ostream& operator<<(std::ostream& os, const SymbolType& type);
std::ostream& operator<<(std::ostream& os, const DataType& type);


#endif // SYMBOL_TABLE_H
