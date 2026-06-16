#pragma once
#include <string>
#include <vector>
#include <sstream>
#include "ast.h"
#include "error.h"

class Transpiler {
public:
    Transpiler(const Program& program,
               const std::string& filename,
               const std::vector<std::string>& c_includes,
               const std::vector<std::string>& cxx_includes,
               ErrorReporter& reporter);

    void compile(const std::string& output_path);
    std::string generate();

private:
    const Program& program;
    std::string filename;
    std::vector<std::string> c_includes;
    std::vector<std::string> cxx_includes;
    ErrorReporter& reporter;
    std::ostringstream out;
    int indent_level;

    // ─── Helpers ────────────────────────────────────────
    void write(const std::string& s);
    void writeln(const std::string& s = "");
    void indent();
    void dedent();
    std::string indentStr();
    std::string badaTypeToCpp(const std::string& type);
    std::string tempFileName(const std::string& output);

    // ─── Top Level ──────────────────────────────────────
    void genIncludes();
    void genStdlibIncludes();
    void genConstDecl(const ConstDecl& c);
    void genExternFn(const ExternFnDecl& e);
    void genStructDecl(const StructDecl& s);
    void genEnumDecl(const EnumDecl& e);
    void genFnDecl(const FnDecl& fn);

    // ─── Statements ─────────────────────────────────────
    void genStmt(const Stmt& stmt);
    void genVarDecl(const VarDeclStmt& stmt);
    void genAssign(const AssignStmt& stmt);
    void genArrayAssign(const ArrayAssignStmt& stmt);
    void genFieldAssign(const FieldAssignStmt& stmt);
    void genIf(const IfStmt& stmt);
    void genWhile(const WhileStmt& stmt);
    void genFor(const ForStmt& stmt);
    void genReturn(const ReturnStmt& stmt);
    void genMultiReturn(const MultiReturnStmt& stmt);
    void genTupleDestruct(const TupleDestructStmt& stmt);
    void genMatch(const MatchStmt& stmt);
    void genAsm(const AsmStmt& stmt);
    void genExprStmt(const ExprStmt& stmt);

    // ─── Expressions ────────────────────────────────────
    std::string genExpr(const Expr& expr);
    std::string genBinOp(const BinOpExpr& expr);
    std::string genCall(const CallExpr& expr);
    std::string genPrintf(const PrintfExpr& expr);
    std::string genIntLit(const IntLitExpr& expr);
    std::string genFloatLit(const FloatLitExpr& expr);
    std::string genStrLit(const StrLitExpr& expr);
    std::string genBoolLit(const BoolLitExpr& expr);
    std::string genIdent(const IdentExpr& expr);
    std::string genStructLit(const StructLitExpr& expr);
    std::string genFieldAccess(const FieldAccessExpr& expr);
    std::string genEnumVariant(const EnumVariantExpr& expr);
    std::string genArrayLit(const ArrayLitExpr& expr);
    std::string genArrayIndex(const ArrayIndexExpr& expr);
    std::string genCast(const CastExpr& expr);
    std::string genRef(const RefExpr& expr);
    std::string genMethodCall(const MethodCallExpr& expr);
    std::string genClosure(const ClosureExpr& expr);
    std::string genTuple(const TupleExpr& expr);
    std::string genResult(const ResultExpr& expr);
    std::string genErrorProp(const ErrorPropExpr& expr);
};
