#pragma once
#include <string>
#include <vector>
#include <memory>

// ─── Forward Declarations ───────────────────────────────
struct Expr;
struct Stmt;

// ─── EXPRESSIONS ────────────────────────────────────────

struct Expr {
    virtual ~Expr() = default;
    int line = 0;
    int col  = 0;
};

struct IntLitExpr : Expr {
    int value;
    IntLitExpr(int v) : value(v) {}
};

struct FloatLitExpr : Expr {
    float value;
    FloatLitExpr(float v) : value(v) {}
};

struct StrLitExpr : Expr {
    std::string value;
    StrLitExpr(std::string v) : value(std::move(v)) {}
};

struct BoolLitExpr : Expr {
    bool value;
    BoolLitExpr(bool v) : value(v) {}
};

struct IdentExpr : Expr {
    std::string name;
    IdentExpr(std::string n) : name(std::move(n)) {}
};

struct BinOpExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    BinOpExpr(std::string o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::string c, std::vector<std::unique_ptr<Expr>> a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct PrintfExpr : Expr {
    std::string format;
    std::vector<std::unique_ptr<Expr>> args;
    bool is_brace_style;
    PrintfExpr(std::string f, std::vector<std::unique_ptr<Expr>> a, bool brace)
        : format(std::move(f)), args(std::move(a)), is_brace_style(brace) {}
};

struct StructLitExpr : Expr {
    std::string struct_name;
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
    StructLitExpr(std::string n,
                  std::vector<std::pair<std::string, std::unique_ptr<Expr>>> f)
        : struct_name(std::move(n)), fields(std::move(f)) {}
};

struct FieldAccessExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string field;
    FieldAccessExpr(std::unique_ptr<Expr> o, std::string f)
        : object(std::move(o)), field(std::move(f)) {}
};

struct EnumVariantExpr : Expr {
    std::string enum_name;
    std::string variant;
    EnumVariantExpr(std::string e, std::string v)
        : enum_name(std::move(e)), variant(std::move(v)) {}
};

struct ArrayLitExpr : Expr {
    std::vector<std::unique_ptr<Expr>> elements;
    std::string elem_type;
    ArrayLitExpr(std::vector<std::unique_ptr<Expr>> e, std::string t = "")
        : elements(std::move(e)), elem_type(std::move(t)) {}
};

struct ArrayIndexExpr : Expr {
    std::unique_ptr<Expr> array;
    std::unique_ptr<Expr> index;
    ArrayIndexExpr(std::unique_ptr<Expr> a, std::unique_ptr<Expr> i)
        : array(std::move(a)), index(std::move(i)) {}
};

struct CastExpr : Expr {
    std::unique_ptr<Expr> value;
    std::string target_type;
    CastExpr(std::unique_ptr<Expr> v, std::string t)
        : value(std::move(v)), target_type(std::move(t)) {}
};

struct RefExpr : Expr {
    std::unique_ptr<Expr> value;
    RefExpr(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct MethodCallExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string method;
    std::vector<std::unique_ptr<Expr>> args;
    MethodCallExpr(std::unique_ptr<Expr> o, std::string m,
                   std::vector<std::unique_ptr<Expr>> a)
        : object(std::move(o)), method(std::move(m)), args(std::move(a)) {}
};

struct AnyExpr : Expr {
    std::unique_ptr<Expr> value;
    AnyExpr(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct StringExpr : Expr {
    std::string value;
    StringExpr(std::string v) : value(std::move(v)) {}
};

struct ClosureExpr : Expr {
    std::vector<std::pair<std::string, std::string>> params;
    std::string return_type;
    std::vector<std::unique_ptr<Stmt>> body;
    ClosureExpr(std::vector<std::pair<std::string, std::string>> p,
                std::string r,
                std::vector<std::unique_ptr<Stmt>> b)
        : params(std::move(p)), return_type(std::move(r)),
          body(std::move(b)) {}
};

struct TupleExpr : Expr {
    std::vector<std::unique_ptr<Expr>> elements;
    TupleExpr(std::vector<std::unique_ptr<Expr>> e)
        : elements(std::move(e)) {}
};

struct ResultExpr : Expr {
    bool is_ok;
    std::unique_ptr<Expr> value;
    ResultExpr(bool ok, std::unique_ptr<Expr> v)
        : is_ok(ok), value(std::move(v)) {}
};

struct ErrorPropExpr : Expr {
    std::unique_ptr<Expr> value;
    ErrorPropExpr(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

// ─── STATEMENTS ─────────────────────────────────────────

struct Stmt {
    virtual ~Stmt() = default;
};

struct VarDeclStmt : Stmt {
    std::string name;
    std::string type;
    bool is_mutable;
    std::unique_ptr<Expr> init;
    VarDeclStmt(std::string n, std::string t, bool m, std::unique_ptr<Expr> i)
        : name(std::move(n)), type(std::move(t)), is_mutable(m), init(std::move(i)) {}
};

struct AssignStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> value;
    AssignStmt(std::string n, std::unique_ptr<Expr> v)
        : name(std::move(n)), value(std::move(v)) {}
};

struct ArrayAssignStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;
    ArrayAssignStmt(std::string n, std::unique_ptr<Expr> i,
                    std::unique_ptr<Expr> v)
        : name(std::move(n)), index(std::move(i)), value(std::move(v)) {}
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value;
    ReturnStmt(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> then_body;
    std::vector<std::unique_ptr<Stmt>> else_body;
    IfStmt(std::unique_ptr<Expr> c,
           std::vector<std::unique_ptr<Stmt>> t,
           std::vector<std::unique_ptr<Stmt>> e)
        : condition(std::move(c)), then_body(std::move(t)), else_body(std::move(e)) {}
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> body;
    WhileStmt(std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Stmt>> b)
        : condition(std::move(c)), body(std::move(b)) {}
};

struct ForStmt : Stmt {
    std::string var;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    bool inclusive;
    std::vector<std::unique_ptr<Stmt>> body;
    ForStmt(std::string v,
            std::unique_ptr<Expr> s,
            std::unique_ptr<Expr> e,
            bool inc,
            std::vector<std::unique_ptr<Stmt>> b)
        : var(std::move(v)), start(std::move(s)), end(std::move(e)),
          inclusive(inc), body(std::move(b)) {}
};

struct BreakStmt : Stmt {
    BreakStmt() = default;
};

struct ContinueStmt : Stmt {
    ContinueStmt() = default;
};

struct AsmStmt : Stmt {
    std::string code;
    AsmStmt(std::string c) : code(std::move(c)) {}
};

struct CxxStmt : Stmt {
    std::string code;
    CxxStmt(std::string c) : code(std::move(c)) {}
};

struct MatchArm {
    std::string enum_name;
    std::string variant;
    std::vector<std::unique_ptr<Stmt>> body;
};

struct MatchStmt : Stmt {
    std::unique_ptr<Expr> value;
    std::vector<MatchArm> arms;
    MatchStmt(std::unique_ptr<Expr> v, std::vector<MatchArm> a)
        : value(std::move(v)), arms(std::move(a)) {}
};

struct FieldAssignStmt : Stmt {
    std::string object;
    std::string field;
    std::unique_ptr<Expr> value;
    FieldAssignStmt(std::string o, std::string f, std::unique_ptr<Expr> v)
        : object(std::move(o)), field(std::move(f)), value(std::move(v)) {}
};

struct MultiReturnStmt : Stmt {
    std::vector<std::unique_ptr<Expr>> values;
    MultiReturnStmt(std::vector<std::unique_ptr<Expr>> v)
        : values(std::move(v)) {}
};

struct TupleDestructStmt : Stmt {
    std::vector<std::string> names;
    std::unique_ptr<Expr> value;
    TupleDestructStmt(std::vector<std::string> n, std::unique_ptr<Expr> v)
        : names(std::move(n)), value(std::move(v)) {}
};

// ─── TOP LEVEL ───────────────────────────────────────────

struct Param {
    std::string type;
    std::string name;
};

struct FnDecl {
    std::string name;
    std::vector<Param> params;
    std::string return_type;
    std::vector<std::string> return_types;
    bool is_nested   = false;
    bool is_error_fn = false;
    std::vector<std::unique_ptr<Stmt>> body;
};

struct StructField {
    std::string type;
    std::string name;
};

struct StructDecl {
    std::string name;
    std::vector<StructField> fields;
    std::vector<FnDecl> methods;
};

struct EnumDecl {
    std::string name;
    std::vector<std::string> variants;
};

struct ConstDecl {
    std::string name;
    std::string type;
    std::unique_ptr<Expr> value;
    ConstDecl(std::string n, std::string t, std::unique_ptr<Expr> v)
        : name(std::move(n)), type(std::move(t)), value(std::move(v)) {}
};

struct ExternFnDecl {
    std::string name;
    std::vector<Param> params;
    std::string return_type;
};

struct UseDecl {
    std::string module;
    std::string submodule;
};

// import math;
// import math as m;
struct ImportDecl {
    std::string module_name;
    std::string alias;
    ImportDecl(std::string n, std::string a = "")
        : module_name(std::move(n)), alias(std::move(a)) {}
};

struct Program {
    std::vector<std::string> includes;
    std::vector<std::string> cxx_includes;
    std::vector<UseDecl> uses;
    std::vector<ImportDecl> imports;
    std::vector<ConstDecl> constants;
    std::vector<ExternFnDecl> extern_fns;
    std::vector<StructDecl> structs;
    std::vector<EnumDecl> enums;
    std::vector<FnDecl> functions;
};
