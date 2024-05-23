#ifndef DEBUG_H
#define DEBUG_H

#include <string>

extern size_t lineNumber;
extern size_t columnNumber;

void trackDebugInfo(const std::string& code, size_t& position, size_t& lineNumber, size_t& columnNumber);

#endif // DEBUG_H
