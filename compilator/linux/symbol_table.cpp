#include "symbol_table.h"
#include <iomanip>





std::ostream& operator<<(std::ostream& os, const SymbolType& type) {
    switch(type) {
        case SymbolType::VARIABLE:
            os << "VARIABLE";
            break;
        case SymbolType::FUNCTION:
            os << "FUNCTION";
            break;
        default:
            os << "UNKNOWN";
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const DataType& type) {
    switch(type) {
        case DataType::INT:
            os << "INT";
            break;
        case DataType::FLOAT:
            os << "FLOAT";
            break;
        case DataType::CHAR:
            os << "CHAR";
            break;
        default:
            os << "UNKNOWN";
    }
    return os;
}


SymbolTable::SymbolTable(){}

void SymbolTable::addSymbol(const std::string& name, SymbolType type, DataType dataType, size_t dataSize) {
    if (symbolExists(name)) {
        throw std::runtime_error("Symbol already exists: " + name);
    }
    symbols[name] = {name, type, dataType, dataSize};
}

bool SymbolTable::symbolExists(const std::string& name) {
    return symbols.find(name) != symbols.end();
}

Symbol SymbolTable::getSymbol(const std::string& name) const {
    auto it = symbols.find(name);
    if (it == symbols.end()) {
        throw std::runtime_error("Symbol not found: " + name);
    }
    return it->second;
}
void SymbolTable::displaySymbolTable(const SymbolTable& symbolTable) {
    // Print header
    std::cout << std::left << std::setw(20) << "Name"
              << std::setw(15) << "Type"
              << std::setw(15) << "Data Type"
              << std::setw(10) << "Data Size" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    // Print each symbol
    for (const auto& entry : symbolTable.symbols) {
        std::cout << std::left << std::setw(20) << entry.first
                  << std::setw(15) << entry.second.type
                  << std::setw(15) << entry.second.dataType
                  << std::setw(10) << entry.second.size << std::endl;
    }
}

DataType SymbolTable::getSymbolDataType(const std::string& name) const 
{
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second.dataType;
    }
    throw std::runtime_error("Symbol not found: " + name);
}

void SymbolTable::updateSymbolSize(const std::string& name, size_t newSize) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        it->second.size = newSize;
    }
}