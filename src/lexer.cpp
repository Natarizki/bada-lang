#include "../include/lexer.h"
#include <stdexcept>
#include <cctype>

Lexer::Lexer(const std::string& source,
             const std::string& filename,
             ErrorReporter& reporter)
    : src(source), filename(filename), reporter(reporter),
      pos(0), line(1), col(1) {}

char Lexer::current() {
    if (pos >= (int)src.size()) return '\0';
    return src[pos];
}

char Lexer::peek(int offset) {
    int p = pos + offset;
    if (p >= (int)src.size()) return '\0';
    return src[p];
}

char Lexer::advance() {
    char c = current();
    pos++;
    if (c == '\n') { line++; col = 1; }
    else col++;
    return c;
}

void Lexer::skipWhitespace() {
    while (pos < (int)src.size() && std::isspace(current()))
        advance();
}

void Lexer::skipComment() {
    if (current() == '/' && peek() == '/') {
        while (current() != '\n' && current() != '\0') advance();
        return;
    }
    if (current() == '*' && peek() == '*') {
        advance(); advance();
        while (!(current() == '*' && peek() == '*') && current() != '\0')
            advance();
        if (current() != '\0') { advance(); advance(); }
        return;
    }
    if (current() == '*') {
        while (current() != '\n' && current() != '\0') advance();
        return;
    }
}

Token Lexer::makeToken(TokenType type, std::string value) {
    return Token(type, std::move(value), line, col);
}

Token Lexer::readString() {
    advance();
    std::string result;
    while (current() != '"' && current() != '\0') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                default:   result += current(); break;
            }
        } else {
            result += current();
        }
        advance();
    }
    advance();
    return makeToken(TokenType::STR_LIT, result);
}

Token Lexer::readNumber() {
    std::string result;
    bool is_float = false;

    while (std::isdigit(current())) {
        result += current();
        advance();
    }

    if (current() == '.' && peek() != '.') {
        is_float = true;
        result += current();
        advance();
        while (std::isdigit(current())) {
            result += current();
            advance();
        }
    }

    if (is_float)
        return makeToken(TokenType::FLOAT_LIT, result);
    return makeToken(TokenType::INT_LIT, result);
}

Token Lexer::readIdent() {
    std::string result;
    while (std::isalnum(current()) || current() == '_') {
        result += current();
        advance();
    }

    if (result == "fn")       return makeToken(TokenType::FN,          result);
    if (result == "let")      return makeToken(TokenType::LET,         result);
    if (result == "mut")      return makeToken(TokenType::MUT,         result);
    if (result == "if")       return makeToken(TokenType::IF,          result);
    if (result == "else")     return makeToken(TokenType::ELSE,        result);
    if (result == "for")      return makeToken(TokenType::FOR,         result);
    if (result == "in")       return makeToken(TokenType::IN,          result);
    if (result == "while")    return makeToken(TokenType::WHILE,       result);
    if (result == "return")   return makeToken(TokenType::RETURN,      result);
    if (result == "true")     return makeToken(TokenType::TRUE,        result);
    if (result == "false")    return makeToken(TokenType::FALSE,       result);
    if (result == "asm")      return makeToken(TokenType::ASM,         result);
    if (result == "cxx")      return makeToken(TokenType::CXX,         result);
    if (result == "struct")   return makeToken(TokenType::STRUCT,      result);
    if (result == "enum")     return makeToken(TokenType::ENUM,        result);
    if (result == "match")    return makeToken(TokenType::MATCH,       result);
    if (result == "const")    return makeToken(TokenType::CONST,       result);
    if (result == "extern")   return makeToken(TokenType::EXTERN,      result);
    if (result == "break")    return makeToken(TokenType::BREAK,       result);
    if (result == "continue") return makeToken(TokenType::CONTINUE,    result);
    if (result == "use")      return makeToken(TokenType::USE,         result);
    if (result == "as")       return makeToken(TokenType::AS,          result);
    if (result == "any")      return makeToken(TokenType::ANY,         result);
    if (result == "String")   return makeToken(TokenType::STRING_TYPE, result);
    if (result == "Ok")       return makeToken(TokenType::OK,          result);
    if (result == "Err")      return makeToken(TokenType::ERR,         result);
    if (result == "import")   return makeToken(TokenType::IMPORT,      result);

    if (result == "int")    return makeToken(TokenType::TYPE_INT,   result);
    if (result == "float")  return makeToken(TokenType::TYPE_FLOAT, result);
    if (result == "str")    return makeToken(TokenType::TYPE_STR,   result);
    if (result == "bool")   return makeToken(TokenType::TYPE_BOOL,  result);

    return makeToken(TokenType::IDENT, result);
}

void Lexer::readInclude() {
    advance();
    while (current() == ' ' || current() == '\t') advance();

    std::string keyword;
    while (std::isalpha(current())) {
        keyword += current();
        advance();
    }
    if (keyword != "include") {
        while (current() != '\n' && current() != '\0') advance();
        return;
    }

    while (current() == ' ' || current() == '\t') advance();

    std::string path;
    if (current() == '<') {
        advance();
        while (current() != '>' && current() != '\0') {
            path += current();
            advance();
        }
        if (current() == '>') advance();
    } else if (current() == '"') {
        advance();
        while (current() != '"' && current() != '\0') {
            path += current();
            advance();
        }
        if (current() == '"') advance();
    }

    bool is_cxx = (path.find("iostream")  != std::string::npos ||
                   path.find("vector")    != std::string::npos ||
                   path.find("string")    != std::string::npos ||
                   path.find("map")       != std::string::npos ||
                   path.find("algorithm") != std::string::npos ||
                   path.find("set")       != std::string::npos ||
                   path.find("queue")     != std::string::npos ||
                   path.find("stack")     != std::string::npos ||
                   path.find("memory")    != std::string::npos ||
                   path.find("thread")    != std::string::npos ||
                   path.find("mutex")     != std::string::npos ||
                   path.find("fstream")   != std::string::npos ||
                   path.find("sstream")   != std::string::npos ||
                   path.find(".hpp")      != std::string::npos ||
                   path.find(".hxx")      != std::string::npos);

    if (is_cxx)
        cxx_includes.push_back(path);
    else
        c_includes.push_back(path);
}

Token Lexer::readAsmBlock() {
    while (current() == ' ' || current() == '\t') advance();
    if (current() == '{') advance();

    std::string code;
    int depth = 1;
    while (current() != '\0' && depth > 0) {
        if (current() == '{') depth++;
        else if (current() == '}') {
            depth--;
            if (depth == 0) { advance(); break; }
        }
        code += current();
        advance();
    }
    return makeToken(TokenType::ASM, code);
}

Token Lexer::readCxxBlock() {
    while (current() == ' ' || current() == '\t') advance();
    if (current() == '{') advance();

    std::string code;
    int depth = 1;
    while (current() != '\0' && depth > 0) {
        if (current() == '{') depth++;
        else if (current() == '}') {
            depth--;
            if (depth == 0) { advance(); break; }
        }
        code += current();
        advance();
    }
    return makeToken(TokenType::CXX, code);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (pos < (int)src.size()) {
        skipWhitespace();
        if (pos >= (int)src.size()) break;

        char c = current();

        if (c == '/' && peek() == '/') { skipComment(); continue; }
        if (c == '*' && peek() == '*') { skipComment(); continue; }
        if (c == '#') { readInclude(); continue; }

        if (c == 'p' && src.substr(pos, 7) == "printf!") {
            for (int i = 0; i < 7; i++) advance();
            tokens.push_back(makeToken(TokenType::PRINTF_MACRO, "printf!"));
            continue;
        }

        if (c == '"') { tokens.push_back(readString()); continue; }
        if (std::isdigit(c)) { tokens.push_back(readNumber()); continue; }

        if (std::isalpha(c) || c == '_') {
            Token t = readIdent();
            if (t.type == TokenType::ASM)
                tokens.push_back(readAsmBlock());
            else if (t.type == TokenType::CXX)
                tokens.push_back(readCxxBlock());
            else
                tokens.push_back(t);
            continue;
        }

        advance();
        switch (c) {
            case '+': tokens.push_back(makeToken(TokenType::PLUS,      "+")); break;
            case '-':
                if (current() == '>') {
                    advance();
                    tokens.push_back(makeToken(TokenType::ARROW, "->"));
                } else {
                    tokens.push_back(makeToken(TokenType::MINUS, "-"));
                }
                break;
            case '/': tokens.push_back(makeToken(TokenType::SLASH,    "/")); break;
            case '%': tokens.push_back(makeToken(TokenType::PERCENT,  "%")); break;
            case '(': tokens.push_back(makeToken(TokenType::LPAREN,   "(")); break;
            case ')': tokens.push_back(makeToken(TokenType::RPAREN,   ")")); break;
            case '{': tokens.push_back(makeToken(TokenType::LBRACE,   "{")); break;
            case '}': tokens.push_back(makeToken(TokenType::RBRACE,   "}")); break;
            case '[': tokens.push_back(makeToken(TokenType::LBRACKET, "[")); break;
            case ']': tokens.push_back(makeToken(TokenType::RBRACKET, "]")); break;
            case ';': tokens.push_back(makeToken(TokenType::SEMICOLON,";")); break;
            case ',': tokens.push_back(makeToken(TokenType::COMMA,    ",")); break;
            case '.':
                if (current() == '.' && peek() == '=') {
                    advance(); advance();
                    tokens.push_back(makeToken(TokenType::DOTDOTEQ, "..="));
                } else if (current() == '.') {
                    advance();
                    tokens.push_back(makeToken(TokenType::DOTDOT, ".."));
                } else {
                    tokens.push_back(makeToken(TokenType::DOT, "."));
                }
                break;
            case ':':
                if (current() == ':') {
                    advance();
                    tokens.push_back(makeToken(TokenType::COLONCOLON, "::"));
                } else {
                    tokens.push_back(makeToken(TokenType::COLON, ":"));
                }
                break;
            case '?': tokens.push_back(makeToken(TokenType::QUESTION,  "?")); break;
            case '!':
                if (current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::NEQ, "!="));
                } else {
                    tokens.push_back(makeToken(TokenType::BANG, "!"));
                }
                break;
            case '=':
                if (current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::EQEQ, "=="));
                } else if (current() == '>') {
                    advance();
                    tokens.push_back(makeToken(TokenType::FATARROW, "=>"));
                } else {
                    tokens.push_back(makeToken(TokenType::EQ, "="));
                }
                break;
            case '<':
                if (current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::LEQ, "<="));
                } else {
                    tokens.push_back(makeToken(TokenType::LT, "<"));
                }
                break;
            case '>':
                if (current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::GEQ, ">="));
                } else {
                    tokens.push_back(makeToken(TokenType::GT, ">"));
                }
                break;
            case '&':
                if (current() == '&') {
                    advance();
                    tokens.push_back(makeToken(TokenType::AND, "&&"));
                } else {
                    tokens.push_back(makeToken(TokenType::AMP, "&"));
                }
                break;
            case '|':
                if (current() == '|') {
                    advance();
                    tokens.push_back(makeToken(TokenType::OR, "||"));
                }
                break;
            case '*':
                tokens.push_back(makeToken(TokenType::STAR, "*"));
                break;
            default:
                tokens.push_back(makeToken(TokenType::UNKNOWN,
                                           std::string(1, c)));
                break;
        }
    }

    tokens.push_back(makeToken(TokenType::EOF_TOKEN, ""));
    return tokens;
}
