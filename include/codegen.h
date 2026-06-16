#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "ast.h"
#include "error.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LegacyPassManager.h"

class Codegen {
public:
    Codegen(ErrorReporter& reporter);
    void generate(const Program& program, const std::string& output_path);

private:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    ErrorReporter& reporter;

    // Symbol tables
    std::unordered_map<std::string, llvm::AllocaInst*> named_values;
    std::unordered_map<std::string, llvm::StructType*> struct_types;
    std::unordered_map<std::string,
        std::unordered_map<std::string, int>> struct_fields;
    std::unordered_map<std::string,
        std::unordered_map<std::string, int>> enum_variants;
    std::unordered_map<std::string, std::string> var_struct_type;
    std::unordered_map<std::string, llvm::Value*> constants;
    std::unordered_map<std::string, llvm::Type*> array_elem_types;

    // Break/continue targets
    llvm::BasicBlock* break_target    = nullptr;
    llvm::BasicBlock* continue_target = nullptr;

    // Closure counter for unique names
    int closure_counter = 0;

    // ─── Helpers ────────────────────────────────────────
    llvm::Type* resolveType(const std::string& type_name);
    llvm::AllocaInst* createEntryAlloca(llvm::Function* fn,
                                         const std::string& name,
                                         llvm::Type* type);
    std::string convertBraceFormat(const std::string& fmt,
                                    const std::vector<llvm::Value*>& args);
    std::string getAsmClobbers();

    // ─── Top Level ──────────────────────────────────────
    void genStructDecl(const StructDecl& s);
    void genEnumDecl(const EnumDecl& e);
    void genConstDecl(const ConstDecl& c);
    void genExternFn(const ExternFnDecl& e);
    void genFunction(const FnDecl& fn);
    llvm::Function* declarePrintf();

    // ─── Statements ─────────────────────────────────────
    void genStmt(const Stmt& stmt);
    void genVarDecl(const VarDeclStmt& stmt);
    void genAssign(const AssignStmt& stmt);
    void genArrayAssign(const ArrayAssignStmt& stmt);
    void genFieldAssign(const FieldAssignStmt& stmt);
    void genIf(const IfStmt& stmt);
    void genWhile(const WhileStmt& stmt);
    void genFor(const ForStmt& stmt);
    void genAsm(const AsmStmt& stmt);
    void genReturn(const ReturnStmt& stmt);
    void genMultiReturn(const MultiReturnStmt& stmt);
    void genTupleDestruct(const TupleDestructStmt& stmt);
    void genMatch(const MatchStmt& stmt);
    void genExprStmt(const ExprStmt& stmt);

    // ─── Expressions ────────────────────────────────────
    llvm::Value* genExpr(const Expr& expr);
    llvm::Value* genBinOp(const BinOpExpr& expr);
    llvm::Value* genCall(const CallExpr& expr);
    llvm::Value* genPrintf(const PrintfExpr& expr);
    llvm::Value* genIntLit(const IntLitExpr& expr);
    llvm::Value* genFloatLit(const FloatLitExpr& expr);
    llvm::Value* genStrLit(const StrLitExpr& expr);
    llvm::Value* genBoolLit(const BoolLitExpr& expr);
    llvm::Value* genIdent(const IdentExpr& expr);
    llvm::Value* genStructLit(const StructLitExpr& expr);
    llvm::Value* genFieldAccess(const FieldAccessExpr& expr);
    llvm::Value* genEnumVariant(const EnumVariantExpr& expr);
    llvm::Value* genArrayLit(const ArrayLitExpr& expr);
    llvm::Value* genArrayIndex(const ArrayIndexExpr& expr);
    llvm::Value* genCast(const CastExpr& expr);
    llvm::Value* genRef(const RefExpr& expr);
    llvm::Value* genMethodCall(const MethodCallExpr& expr);
    llvm::Value* genClosure(const ClosureExpr& expr);
    llvm::Value* genTuple(const TupleExpr& expr);
    llvm::Value* genResult(const ResultExpr& expr);
    llvm::Value* genErrorProp(const ErrorPropExpr& expr);

    // ─── Output ─────────────────────────────────────────
    void compileToObject(const std::string& output_path,
                          const std::vector<std::string>& c_includes,
                          const std::vector<std::string>& cxx_includes);
};
