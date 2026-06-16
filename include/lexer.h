#pragma once
#include <string>
#include <vector>
#include "error.h"

// ─── TOKEN TYPES ────────────────────────────────────────
enum class TokenType {
    // Literals
    INT_LIT, FLOAT_LIT, STR_LIT, BOOL_LIT,

    // Identifiers & Keywords
    IDENT,
    FN, LET, MUT, IF, ELSE, FOR, IN, WHILE,
    RETURN, TRUE, FALSE, INCLUDE, ASM, CXX,
    STRUCT, ENUM, MATCH,
    CONST, EXTERN, BREAK, CONTINUE, USE, AS,
    ANY, STRING_TYPE, OK, ERR, IMPORT,

    // Types
    TYPE_INT, TYPE_FLOAT, TYPE_STR, TYPE_BOOL,

    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, EQEQ, NEQ, LT, GT, LEQ, GEQ,
    AND, OR, NOT, AMP,

    // Punctuation
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    SEMICOLON, COLON, COLONCOLON, COMMA, DOT,
    ARROW, FATARROW, DOTDOT, DOTDOTEQ, QUESTION,
    BANG,

    // Special
    PRINTF_MACRO,
    EOF_TOKEN,
    UNKNOWN
};

// ─── TOKEN ──────────────────────────────────────────────
struct Token {
    TokenType type;
    std::string value;
    int line;
    int col;

    Token(TokenType t, std::string v, int l, int c)
        : type(t), value(std::move(v)), line(l), col(c) {}
};

// ─── LEXER ──────────────────────────────────────────────
class Lexer {
public:
    Lexer(const std::string& source,
          const std::string& filename,
          ErrorReporter& reporter);

    std::vector<Token> tokenize();

    std::vector<std::string> c_includes;
    std::vector<std::string> cxx_includes;

private:
    std::string src;
    std::string filename;
    ErrorReporter& reporter;
    int pos;
    int line;
    int col;

    char current();
    char peek(int offset = 1);
    char advance();
    void skipWhitespace();
    void skipComment();

    Token makeToken(TokenType type, std::string value);

    Token readString();
    Token readNumber();
    Token readIdent();
    void  readInclude();
    Token readAsmBlock();
    Token readCxxBlock();
};
