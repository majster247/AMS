#include "debug.h"

size_t lineNumber = 1;
size_t columnNumber = 1;

void trackDebugInfo(const std::string& code, size_t& position, size_t& lineNumber, size_t& columnNumber) {
    lineNumber = 1;
    columnNumber = 1;
    for (size_t i = 0; i < position; ++i) {
        if (code[i] == '\n') {
            ++lineNumber;
            columnNumber = 1;
        } else {
            ++columnNumber;
        }
    }
}
