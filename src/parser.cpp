#include "../include/parser.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens, ErrorReporter& reporter)
    : tokens(std::move(tokens)), pos(0), reporter(reporter) {}

// ─── Token Helpers ──────────────────────────────────────

Token& Parser::current() { return tokens[pos]; }

Token& Parser::peek(int offset) {
    int p = pos + offset;
    if (p >= (int)tokens.size()) return tokens.back();
    return tokens[p];
}

Token& Parser::advance() {
    Token& t = tokens[pos];
    if (pos < (int)tokens.size() - 1) pos++;
    return t;
}

bool Parser::check(TokenType type) {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenType type, const std::string& msg) {
    if (!check(type)) {
        reporter.error(
            ErrorCode::E003_UNEXPECTED_TOKEN,
            msg + " (got '" + current().value + "')",
            current().line, current().col
        );
    }
    return advance();
}

// ─── Top Level ──────────────────────────────────────────

Program Parser::parse() {
    Program program;
    while (!check(TokenType::EOF_TOKEN)) {
        if (check(TokenType::FN)) {
            program.functions.push_back(parseFnDecl());
        } else if (check(TokenType::STRUCT)) {
            program.structs.push_back(parseStructDecl());
        } else if (check(TokenType::ENUM)) {
            program.enums.push_back(parseEnumDecl());
        } else if (check(TokenType::CONST)) {
            program.constants.push_back(parseConstDecl());
        } else if (check(TokenType::EXTERN)) {
            program.extern_fns.push_back(parseExternFnDecl());
        } else if (check(TokenType::USE)) {
            program.uses.push_back(parseUseDecl());
        } else if (check(TokenType::IMPORT)) {
            program.imports.push_back(parseImportDecl());
        } else {
            advance();
        }
    }
    return program;
}

FnDecl Parser::parseFnDecl(bool is_nested) {
    expect(TokenType::FN, "expected 'fn'");
    FnDecl fn;
    fn.name      = expect(TokenType::IDENT, "expected function name").value;
    fn.is_nested = is_nested;

    expect(TokenType::LPAREN, "expected '('");
    fn.params = parseParams();
    expect(TokenType::RPAREN, "expected ')'");

    if (check(TokenType::ARROW)) {
        advance();
        std::string first = current().value;
        advance();
        if (check(TokenType::COMMA)) {
            fn.return_types.push_back(first);
            while (match(TokenType::COMMA)) {
                fn.return_types.push_back(current().value);
                advance();
            }
        } else if (check(TokenType::BANG)) {
            fn.return_type  = first;
            fn.is_error_fn  = true;
            advance();
        } else {
            fn.return_type = first;
        }
    } else {
        fn.return_type = "void";
    }

    expect(TokenType::LBRACE, "expected '{'");
    fn.body = parseBlock();
    expect(TokenType::RBRACE, "expected '}'");

    return fn;
}

StructDecl Parser::parseStructDecl() {
    expect(TokenType::STRUCT, "expected 'struct'");
    StructDecl s;
    s.name = expect(TokenType::IDENT, "expected struct name").value;

    expect(TokenType::LBRACE, "expected '{'");
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        if (check(TokenType::FN)) {
            s.methods.push_back(parseFnDecl());
            continue;
        }
        StructField field;
        field.type = current().value;
        advance();
        field.name = expect(TokenType::IDENT, "expected field name").value;
        expect(TokenType::SEMICOLON, "expected ';'");
        s.fields.push_back(field);
    }
    expect(TokenType::RBRACE, "expected '}'");
    return s;
}

EnumDecl Parser::parseEnumDecl() {
    expect(TokenType::ENUM, "expected 'enum'");
    EnumDecl e;
    e.name = expect(TokenType::IDENT, "expected enum name").value;

    expect(TokenType::LBRACE, "expected '{'");
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        e.variants.push_back(
            expect(TokenType::IDENT, "expected variant name").value
        );
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RBRACE, "expected '}'");
    return e;
}

ConstDecl Parser::parseConstDecl() {
    expect(TokenType::CONST, "expected 'const'");
    std::string name = expect(TokenType::IDENT, "expected name").value;
    expect(TokenType::COLON, "expected ':'");
    std::string type = current().value;
    advance();
    expect(TokenType::EQ, "expected '='");
    auto value = parseExpr();
    expect(TokenType::SEMICOLON, "expected ';'");
    return ConstDecl(name, type, std::move(value));
}

ExternFnDecl Parser::parseExternFnDecl() {
    expect(TokenType::EXTERN, "expected 'extern'");
    expect(TokenType::FN, "expected 'fn'");
    ExternFnDecl e;
    e.name = expect(TokenType::IDENT, "expected name").value;
    expect(TokenType::LPAREN, "expected '('");
    e.params = parseParams();
    expect(TokenType::RPAREN, "expected ')'");
    e.return_type = parseReturnType();
    expect(TokenType::SEMICOLON, "expected ';'");
    return e;
}

UseDecl Parser::parseUseDecl() {
    expect(TokenType::USE, "expected 'use'");
    UseDecl u;
    u.module = expect(TokenType::IDENT, "expected module name").value;
    expect(TokenType::COLONCOLON, "expected '::'");
    u.submodule = expect(TokenType::IDENT, "expected submodule name").value;
    expect(TokenType::SEMICOLON, "expected ';'");
    return u;
}

ImportDecl Parser::parseImportDecl() {
    expect(TokenType::IMPORT, "expected 'import'");
    std::string name = expect(TokenType::IDENT,
        "expected module name").value;

    std::string alias = "";
    if (match(TokenType::AS)) {
        alias = expect(TokenType::IDENT,
            "expected alias name").value;
    }

    expect(TokenType::SEMICOLON, "expected ';'");
    return ImportDecl(name, alias);
}

std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
        Param p;
        p.type = current().value;
        advance();
        p.name = expect(TokenType::IDENT, "expected param name").value;
        params.push_back(p);
        if (!match(TokenType::COMMA)) break;
    }
    return params;
}

std::string Parser::parseReturnType() {
    if (match(TokenType::ARROW)) {
        std::string rt = current().value;
        advance();
        return rt;
    }
    return "void";
}

// ─── Statements ─────────────────────────────────────────

std::vector<std::unique_ptr<Stmt>> Parser::parseBlock() {
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        stmts.push_back(parseStmt());
    }
    return stmts;
}

std::unique_ptr<Stmt> Parser::parseStmt() {
    if (check(TokenType::LET))      return parseVarDecl();
    if (check(TokenType::IF))       return parseIfStmt();
    if (check(TokenType::WHILE))    return parseWhileStmt();
    if (check(TokenType::FOR))      return parseForStmt();
    if (check(TokenType::RETURN))   return parseReturnStmt();
    if (check(TokenType::ASM))      return parseAsmStmt();
    if (check(TokenType::CXX))      return parseCxxStmt();
    if (check(TokenType::MATCH))    return parseMatchStmt();
    if (check(TokenType::BREAK))    return parseBreakStmt();
    if (check(TokenType::CONTINUE)) return parseContinueStmt();
    if (check(TokenType::FN)) {
        auto fn = parseFnDecl(true);
        return std::make_unique<VarDeclStmt>(
            "__nested_fn_" + fn.name, "fn", false, nullptr
        );
    }
    return parseExprStmt();
}

std::unique_ptr<Stmt> Parser::parseVarDecl() {
    expect(TokenType::LET, "expected 'let'");

    bool is_mut = false;
    if (check(TokenType::MUT)) { advance(); is_mut = true; }

    // Tuple destructure: let (a, b) = ...
    if (check(TokenType::LPAREN)) {
        advance();
        std::vector<std::string> names;
        while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
            names.push_back(
                expect(TokenType::IDENT, "expected variable name").value
            );
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RPAREN, "expected ')'");
        expect(TokenType::EQ, "expected '='");
        auto val = parseExpr();
        expect(TokenType::SEMICOLON, "expected ';'");
        return std::make_unique<TupleDestructStmt>(
            std::move(names), std::move(val)
        );
    }

    std::string name = expect(TokenType::IDENT,
        "expected variable name").value;

    std::string type = "auto";
    if (match(TokenType::COLON)) {
        if (check(TokenType::LBRACKET)) {
            advance();
            type = "[" + current().value + "]";
            advance();
            expect(TokenType::RBRACKET, "expected ']'");
        } else {
            type = current().value;
            advance();
        }
    }

    expect(TokenType::EQ, "expected '='");
    auto init = parseExpr();
    expect(TokenType::SEMICOLON, "expected ';'");

    return std::make_unique<VarDeclStmt>(name, type, is_mut,
                                         std::move(init));
}

std::unique_ptr<Stmt> Parser::parseIfStmt() {
    expect(TokenType::IF, "expected 'if'");
    auto cond = parseExpr();
    expect(TokenType::LBRACE, "expected '{'");
    auto then_body = parseBlock();
    expect(TokenType::RBRACE, "expected '}'");

    std::vector<std::unique_ptr<Stmt>> else_body;
    if (match(TokenType::ELSE)) {
        expect(TokenType::LBRACE, "expected '{'");
        else_body = parseBlock();
        expect(TokenType::RBRACE, "expected '}'");
    }

    return std::make_unique<IfStmt>(std::move(cond),
                                    std::move(then_body),
                                    std::move(else_body));
}

std::unique_ptr<Stmt> Parser::parseWhileStmt() {
    expect(TokenType::WHILE, "expected 'while'");
    auto cond = parseExpr();
    expect(TokenType::LBRACE, "expected '{'");
    auto body = parseBlock();
    expect(TokenType::RBRACE, "expected '}'");
    return std::make_unique<WhileStmt>(std::move(cond),
                                       std::move(body));
}

std::unique_ptr<Stmt> Parser::parseForStmt() {
    expect(TokenType::FOR, "expected 'for'");
    std::string var = expect(TokenType::IDENT,
        "expected loop variable").value;
    expect(TokenType::IN, "expected 'in'");

    auto start = parseExpr();

    bool inclusive = false;
    if (check(TokenType::DOTDOTEQ)) {
        inclusive = true; advance();
    } else {
        expect(TokenType::DOTDOT, "expected '..' or '..='");
    }

    auto end = parseExpr();
    expect(TokenType::LBRACE, "expected '{'");
    auto body = parseBlock();
    expect(TokenType::RBRACE, "expected '}'");

    return std::make_unique<ForStmt>(var, std::move(start),
                                     std::move(end), inclusive,
                                     std::move(body));
}

std::unique_ptr<Stmt> Parser::parseReturnStmt() {
    expect(TokenType::RETURN, "expected 'return'");

    auto first = parseExpr();
    if (check(TokenType::COMMA)) {
        std::vector<std::unique_ptr<Expr>> values;
        values.push_back(std::move(first));
        while (match(TokenType::COMMA))
            values.push_back(parseExpr());
        expect(TokenType::SEMICOLON, "expected ';'");
        return std::make_unique<MultiReturnStmt>(std::move(values));
    }

    expect(TokenType::SEMICOLON, "expected ';'");
    return std::make_unique<ReturnStmt>(std::move(first));
}

std::unique_ptr<Stmt> Parser::parseAsmStmt() {
    std::string code = current().value;
    advance();
    return std::make_unique<AsmStmt>(code);
}

std::unique_ptr<Stmt> Parser::parseCxxStmt() {
    std::string code = current().value;
    advance();
    return std::make_unique<CxxStmt>(code);
}

std::unique_ptr<Stmt> Parser::parseBreakStmt() {
    expect(TokenType::BREAK, "expected 'break'");
    match(TokenType::SEMICOLON);
    return std::make_unique<BreakStmt>();
}

std::unique_ptr<Stmt> Parser::parseContinueStmt() {
    expect(TokenType::CONTINUE, "expected 'continue'");
    match(TokenType::SEMICOLON);
    return std::make_unique<ContinueStmt>();
}

std::unique_ptr<Stmt> Parser::parseMatchStmt() {
    expect(TokenType::MATCH, "expected 'match'");
    auto value = parseExpr();

    expect(TokenType::LBRACE, "expected '{'");
    std::vector<MatchArm> arms;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        MatchArm arm;
        arm.enum_name = expect(TokenType::IDENT,
            "expected enum name").value;
        expect(TokenType::COLONCOLON, "expected '::'");
        arm.variant = expect(TokenType::IDENT,
            "expected variant name").value;
        expect(TokenType::FATARROW, "expected '=>'");

        if (check(TokenType::LBRACE)) {
            advance();
            arm.body = parseBlock();
            expect(TokenType::RBRACE, "expected '}'");
        } else {
            arm.body.push_back(parseStmt());
        }

        arms.push_back(std::move(arm));
        match(TokenType::COMMA);
    }

    expect(TokenType::RBRACE, "expected '}'");
    return std::make_unique<MatchStmt>(std::move(value),
                                       std::move(arms));
}

std::unique_ptr<Stmt> Parser::parseExprStmt() {
    // Array assign: ident[idx] = expr;
    if (check(TokenType::IDENT) &&
        peek(1).type == TokenType::LBRACKET) {
        int saved_pos = pos;
        std::string name = advance().value;
        advance(); // [
        auto idx = parseExpr();
        expect(TokenType::RBRACKET, "expected ']'");
        if (check(TokenType::EQ)) {
            advance();
            auto val = parseExpr();
            expect(TokenType::SEMICOLON, "expected ';'");
            return std::make_unique<ArrayAssignStmt>(
                name, std::move(idx), std::move(val)
            );
        }
        pos = saved_pos;
    }

    // Field assign: ident.field = expr;
    if (check(TokenType::IDENT) &&
        peek(1).type == TokenType::DOT &&
        peek(2).type == TokenType::IDENT &&
        peek(3).type == TokenType::EQ) {
        std::string obj   = advance().value;
        advance(); // .
        std::string field = advance().value;
        advance(); // =
        auto val = parseExpr();
        expect(TokenType::SEMICOLON, "expected ';'");
        return std::make_unique<FieldAssignStmt>(obj, field,
                                                  std::move(val));
    }

    // Normal assign
    if (check(TokenType::IDENT) && peek().type == TokenType::EQ) {
        std::string name = advance().value;
        advance();
        auto val = parseExpr();
        expect(TokenType::SEMICOLON, "expected ';'");
        return std::make_unique<AssignStmt>(name, std::move(val));
    }

    auto expr = parseExpr();
    match(TokenType::SEMICOLON);
    return std::make_unique<ExprStmt>(std::move(expr));
}

// ─── Expressions ────────────────────────────────────────

std::unique_ptr<Expr> Parser::parseExpr() {
    return parseComparison();
}

std::unique_ptr<Expr> Parser::parseComparison() {
    auto left = parseAddSub();
    while (check(TokenType::EQEQ) || check(TokenType::NEQ) ||
           check(TokenType::LT)   || check(TokenType::GT)  ||
           check(TokenType::LEQ)  || check(TokenType::GEQ)) {
        std::string op = advance().value;
        auto right = parseAddSub();
        left = std::make_unique<BinOpExpr>(op, std::move(left),
                                            std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseAddSub() {
    auto left = parseMulDiv();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = advance().value;
        auto right = parseMulDiv();
        left = std::make_unique<BinOpExpr>(op, std::move(left),
                                            std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseMulDiv() {
    auto left = parseUnary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) ||
           check(TokenType::PERCENT)) {
        std::string op = advance().value;
        auto right = parseUnary();
        left = std::make_unique<BinOpExpr>(op, std::move(left),
                                            std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (check(TokenType::MINUS) || check(TokenType::NOT)) {
        std::string op = advance().value;
        auto operand = parsePrimary();
        return std::make_unique<BinOpExpr>(op, nullptr,
                                            std::move(operand));
    }
    if (check(TokenType::AMP)) {
        advance();
        auto val = parsePrimary();
        return std::make_unique<RefExpr>(std::move(val));
    }
    return parsePrimary();
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (check(TokenType::PRINTF_MACRO)) return parsePrintf();
    if (check(TokenType::LBRACKET))     return parseArrayLit();
    if (check(TokenType::FN))           return parseClosure();
    if (check(TokenType::OK))           return parseResult(true);
    if (check(TokenType::ERR))          return parseResult(false);

    // Tuple or grouped expr
    if (check(TokenType::LPAREN)) {
        advance();
        auto first = parseExpr();
        if (check(TokenType::COMMA)) {
            std::vector<std::unique_ptr<Expr>> elems;
            elems.push_back(std::move(first));
            while (match(TokenType::COMMA)) {
                if (check(TokenType::RPAREN)) break;
                elems.push_back(parseExpr());
            }
            expect(TokenType::RPAREN, "expected ')'");
            return std::make_unique<TupleExpr>(std::move(elems));
        }
        expect(TokenType::RPAREN, "expected ')'");
        if (check(TokenType::AS)) return parseCast(std::move(first));
        return first;
    }

    if (check(TokenType::INT_LIT)) {
        int val = std::stoi(current().value);
        advance();
        auto e = std::make_unique<IntLitExpr>(val);
        if (check(TokenType::AS)) return parseCast(std::move(e));
        return e;
    }
    if (check(TokenType::FLOAT_LIT)) {
        float val = std::stof(current().value);
        advance();
        auto e = std::make_unique<FloatLitExpr>(val);
        if (check(TokenType::AS)) return parseCast(std::move(e));
        return e;
    }
    if (check(TokenType::STR_LIT)) {
        std::string val = current().value;
        advance();
        return std::make_unique<StrLitExpr>(val);
    }
    if (check(TokenType::TRUE)) {
        advance();
        return std::make_unique<BoolLitExpr>(true);
    }
    if (check(TokenType::FALSE)) {
        advance();
        return std::make_unique<BoolLitExpr>(false);
    }

    if (check(TokenType::IDENT)) {
        int l = current().line;
        int c = current().col;
        std::string name = advance().value;

        // Enum variant OR module::function call: Name::Variant or math::add(...)
        if (check(TokenType::COLONCOLON)) {
            advance();
            std::string variant = expect(TokenType::IDENT,
                "expected variant or function name").value;

            // Module function call: math::add(5, 10)
            if (check(TokenType::LPAREN)) {
                // Treat as regular call with mangled name
                std::string mangled = name + "_" + variant;
                return parseCall(mangled);
            }

            // Enum variant
            auto node = std::make_unique<EnumVariantExpr>(name, variant);
            node->line = l; node->col = c;
            return node;
        }

        // Struct literal
        if (check(TokenType::LBRACE) &&
            peek(1).type == TokenType::IDENT &&
            peek(2).type == TokenType::COLON)
            return parseStructLit(name);

          // Function call
        if (check(TokenType::LPAREN))
            return parseCall(name);

        auto node = std::make_unique<IdentExpr>(name);
        node->line = l; node->col = c;

        std::unique_ptr<Expr> result = std::move(node);

        while (true) {
            if (check(TokenType::DOT)) {
                advance();
                std::string field = expect(TokenType::IDENT,
                    "expected field name").value;
                if (check(TokenType::LPAREN)) {
                    advance();
                    std::vector<std::unique_ptr<Expr>> args;
                    while (!check(TokenType::RPAREN) &&
                           !check(TokenType::EOF_TOKEN)) {
                        args.push_back(parseExpr());
                        if (!match(TokenType::COMMA)) break;
                    }
                    expect(TokenType::RPAREN, "expected ')'");
                    result = std::make_unique<MethodCallExpr>(
                        std::move(result), field, std::move(args)
                    );
                } else {
                    result = std::make_unique<FieldAccessExpr>(
                        std::move(result), field
                    );
                }
            } else if (check(TokenType::LBRACKET)) {
                advance();
                auto idx = parseExpr();
                expect(TokenType::RBRACKET, "expected ']'");
                result = std::make_unique<ArrayIndexExpr>(
                    std::move(result), std::move(idx)
                );
            } else if (check(TokenType::QUESTION)) {
                advance();
                result = std::make_unique<ErrorPropExpr>(
                    std::move(result)
                );
            } else {
                break;
            }
        }

        if (check(TokenType::AS))
            return parseCast(std::move(result));

        return result;
    }

    reporter.error(
        ErrorCode::E003_UNEXPECTED_TOKEN,
        "unexpected token '" + current().value + "'",
        current().line, current().col
    );
}

std::unique_ptr<Expr> Parser::parseClosure() {
    expect(TokenType::FN, "expected 'fn'");
    expect(TokenType::LPAREN, "expected '('");

    std::vector<std::pair<std::string, std::string>> params;
    while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
        std::string type = current().value; advance();
        std::string name = expect(TokenType::IDENT,
            "expected param name").value;
        params.push_back({type, name});
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RPAREN, "expected ')'");

    std::string ret = "void";
    if (match(TokenType::ARROW)) {
        ret = current().value; advance();
    }

    expect(TokenType::LBRACE, "expected '{'");
    auto body = parseBlock();
    expect(TokenType::RBRACE, "expected '}'");

    return std::make_unique<ClosureExpr>(
        std::move(params), ret, std::move(body)
    );
}

std::unique_ptr<Expr> Parser::parseResult(bool is_ok) {
    advance();
    expect(TokenType::LPAREN, "expected '('");
    auto val = parseExpr();
    expect(TokenType::RPAREN, "expected ')'");
    return std::make_unique<ResultExpr>(is_ok, std::move(val));
}

std::unique_ptr<Expr> Parser::parseArrayLit() {
    expect(TokenType::LBRACKET, "expected '['");
    std::vector<std::unique_ptr<Expr>> elements;
    while (!check(TokenType::RBRACKET) && !check(TokenType::EOF_TOKEN)) {
        elements.push_back(parseExpr());
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RBRACKET, "expected ']'");
    return std::make_unique<ArrayLitExpr>(std::move(elements));
}

std::unique_ptr<Expr> Parser::parseCast(std::unique_ptr<Expr> value) {
    expect(TokenType::AS, "expected 'as'");
    std::string target = current().value;
    advance();
    return std::make_unique<CastExpr>(std::move(value), target);
}

std::unique_ptr<Expr> Parser::parseStructLit(std::string name) {
    expect(TokenType::LBRACE, "expected '{'");
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        std::string fname = expect(TokenType::IDENT,
            "expected field name").value;
        expect(TokenType::COLON, "expected ':'");
        auto fval = parseExpr();
        fields.push_back({fname, std::move(fval)});
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RBRACE, "expected '}'");
    return std::make_unique<StructLitExpr>(name, std::move(fields));
}

std::unique_ptr<Expr> Parser::parsePrintf() {
    advance();
    expect(TokenType::LPAREN, "expected '('");
    std::string fmt = expect(TokenType::STR_LIT,
        "expected format string").value;
    bool is_brace = (fmt.find("{}") != std::string::npos);

    std::vector<std::unique_ptr<Expr>> args;
    while (match(TokenType::COMMA))
        args.push_back(parseExpr());

    expect(TokenType::RPAREN, "expected ')'");
    match(TokenType::SEMICOLON);
    return std::make_unique<PrintfExpr>(fmt, std::move(args), is_brace);
}

std::unique_ptr<Expr> Parser::parseCall(std::string name) {
    expect(TokenType::LPAREN, "expected '('");
    std::vector<std::unique_ptr<Expr>> args;
    while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
        args.push_back(parseExpr());
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RPAREN, "expected ')'");
    return std::make_unique<CallExpr>(name, std::move(args));
}
