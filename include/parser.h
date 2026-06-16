#pragma once
#include <vector>
#include <memory>
#include <stdexcept>
#include "ast.h"
#include "lexer.h"
#include "error.h"

// ─── PARSER ─────────────────────────────────────────────
class Parser {
public:
    Parser(std::vector<Token> tokens, ErrorReporter& reporter);
    Program parse();

private:
    std::vector<Token> tokens;
    int pos;
    ErrorReporter& reporter;

    // ─── Token Helpers ──────────────────────────────────
    Token& current();
    Token& peek(int offset = 1);
    Token& advance();
    bool check(TokenType type);
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& msg);

    // ─── Top Level ──────────────────────────────────────
    FnDecl parseFnDecl(bool is_nested = false);
    StructDecl parseStructDecl();
    EnumDecl parseEnumDecl();
    ConstDecl parseConstDecl();
    ExternFnDecl parseExternFnDecl();
    UseDecl parseUseDecl();
    ImportDecl parseImportDecl();
    std::vector<Param> parseParams();
    std::string parseReturnType();

    // ─── Statements ─────────────────────────────────────
    std::unique_ptr<Stmt> parseStmt();
    std::unique_ptr<Stmt> parseVarDecl();
    std::unique_ptr<Stmt> parseIfStmt();
    std::unique_ptr<Stmt> parseWhileStmt();
    std::unique_ptr<Stmt> parseForStmt();
    std::unique_ptr<Stmt> parseAsmStmt();
    std::unique_ptr<Stmt> parseCxxStmt();
    std::unique_ptr<Stmt> parseReturnStmt();
    std::unique_ptr<Stmt> parseMatchStmt();
    std::unique_ptr<Stmt> parseBreakStmt();
    std::unique_ptr<Stmt> parseContinueStmt();
    std::unique_ptr<Stmt> parseExprStmt();
    std::vector<std::unique_ptr<Stmt>> parseBlock();

    // ─── Expressions ────────────────────────────────────
    std::unique_ptr<Expr> parseExpr();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseAddSub();
    std::unique_ptr<Expr> parseMulDiv();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Expr> parsePrintf();
    std::unique_ptr<Expr> parseCall(std::string name);
    std::unique_ptr<Expr> parseStructLit(std::string name);
    std::unique_ptr<Expr> parseArrayLit();
    std::unique_ptr<Expr> parseCast(std::unique_ptr<Expr> value);
    std::unique_ptr<Expr> parseClosure();
    std::unique_ptr<Expr> parseResult(bool is_ok);
};
