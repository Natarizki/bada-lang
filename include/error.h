#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

// ─── ERROR CODES ────────────────────────────────────────
enum class ErrorCode {
    E001_UNDEFINED_VAR,
    E002_UNDEFINED_FN,
    E003_UNEXPECTED_TOKEN,
    E004_TYPE_MISMATCH,
    E005_MISSING_SEMICOLON,
    E006_CANNOT_OPEN_FILE,
    E007_MODULE_NOT_FOUND,
    E008_UNKNOWN_OPERATOR,
    E009_UNKNOWN_STATEMENT,
    E010_LLVM_VERIFY_FAILED,
};

// ─── ERROR REPORTER ─────────────────────────────────────
class ErrorReporter {
public:
    ErrorReporter(const std::string& filename, const std::string& source);

    // Report error and throw
    [[noreturn]] void error(ErrorCode code,
                             const std::string& message,
                             int line, int col);

    // Report warning (no throw)
    void warning(const std::string& message, int line, int col);

    // Report error without location
    [[noreturn]] void fatal(ErrorCode code, const std::string& message);

    bool hasErrors() const { return error_count > 0; }
    int errorCount() const { return error_count; }

private:
    std::string filename;
    std::vector<std::string> lines; // source split by line
    int error_count;

    std::string getErrorCode(ErrorCode code);
    std::string getLine(int line_num);
    std::string makePointer(int col, int len = 1);
    void printDiagnostic(const std::string& level,
                          const std::string& code_str,
                          const std::string& message,
                          int line, int col);
};

// ─── BADA EXCEPTION ─────────────────────────────────────
class BadaError : public std::exception {
public:
    BadaError(const std::string& msg) : msg(msg) {}
    const char* what() const noexcept override { return msg.c_str(); }
private:
    std::string msg;
};
