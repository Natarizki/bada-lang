#include "../include/error.h"
#include <sstream>
#include <iostream>

// ANSI colors
#define RED     "\033[1;31m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"
#define BLUE    "\033[1;34m"

ErrorReporter::ErrorReporter(const std::string& filename,
                              const std::string& source)
    : filename(filename), error_count(0) {
    // Split source into lines
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
}

std::string ErrorReporter::getErrorCode(ErrorCode code) {
    switch (code) {
        case ErrorCode::E001_UNDEFINED_VAR:      return "E001";
        case ErrorCode::E002_UNDEFINED_FN:       return "E002";
        case ErrorCode::E003_UNEXPECTED_TOKEN:   return "E003";
        case ErrorCode::E004_TYPE_MISMATCH:      return "E004";
        case ErrorCode::E005_MISSING_SEMICOLON:  return "E005";
        case ErrorCode::E006_CANNOT_OPEN_FILE:   return "E006";
        case ErrorCode::E007_MODULE_NOT_FOUND:   return "E007";
        case ErrorCode::E008_UNKNOWN_OPERATOR:   return "E008";
        case ErrorCode::E009_UNKNOWN_STATEMENT:  return "E009";
        case ErrorCode::E010_LLVM_VERIFY_FAILED: return "E010";
        default: return "E000";
    }
}

std::string ErrorReporter::getLine(int line_num) {
    if (line_num < 1 || line_num > (int)lines.size())
        return "";
    return lines[line_num - 1];
}

std::string ErrorReporter::makePointer(int col, int len) {
    std::string ptr;
    for (int i = 1; i < col; i++) ptr += ' ';
    for (int i = 0; i < len; i++) ptr += '^';
    return ptr;
}

void ErrorReporter::printDiagnostic(const std::string& level,
                                     const std::string& code_str,
                                     const std::string& message,
                                     int line, int col) {
    // error[E001]: message
    std::cerr << RED << level << "[" << code_str << "]"
              << RESET << ": " << BOLD << message << RESET << "\n";

    // --> filename:line:col
    std::cerr << BLUE << " --> " << RESET
              << filename << ":" << line << ":" << col << "\n";

    // Separator
    std::cerr << BLUE << "  |" << RESET << "\n";

    // Source line
    std::string src_line = getLine(line);
    std::cerr << BLUE << std::to_string(line) << " |" << RESET
              << " " << src_line << "\n";

    // Pointer
    std::cerr << BLUE << "  |" << RESET
              << " " << RED << makePointer(col) << RESET << "\n";

    std::cerr << "\n";
}

void ErrorReporter::error(ErrorCode code,
                           const std::string& message,
                           int line, int col) {
    error_count++;
    printDiagnostic("error", getErrorCode(code), message, line, col);
    throw BadaError(message);
}

void ErrorReporter::warning(const std::string& message,
                              int line, int col) {
    std::cerr << YELLOW << "warning" << RESET
              << ": " << BOLD << message << RESET << "\n";
    std::cerr << BLUE << " --> " << RESET
              << filename << ":" << line << ":" << col << "\n";
    std::cerr << BLUE << "  |" << RESET << "\n";
    std::cerr << BLUE << std::to_string(line) << " |" << RESET
              << " " << getLine(line) << "\n";
    std::cerr << BLUE << "  |" << RESET << "\n\n";
}

void ErrorReporter::fatal(ErrorCode code, const std::string& message) {
    error_count++;
    std::cerr << RED << "error[" << getErrorCode(code) << "]"
              << RESET << ": " << BOLD << message << RESET << "\n\n";
    throw BadaError(message);
}
