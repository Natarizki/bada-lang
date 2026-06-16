#include "../include/codegen.h"
#include <stdexcept>
#include <iostream>
#include <cstdio>
#include <sstream>

#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

Codegen::Codegen(ErrorReporter& reporter)
    : module(std::make_unique<llvm::Module>("bada_module", context)),
      builder(std::make_unique<llvm::IRBuilder<>>(context)),
      reporter(reporter) {}

// ─── Helpers ────────────────────────────────────────────

llvm::Type* Codegen::resolveType(const std::string& type_name) {
    if (type_name == "int" || type_name == "auto")
        return llvm::Type::getInt32Ty(context);
    if (type_name == "float")
        return llvm::Type::getFloatTy(context);
    if (type_name == "bool")
        return llvm::Type::getInt1Ty(context);
    if (type_name == "str" || type_name == "String" || type_name == "any")
        return llvm::PointerType::get(context, 0);
    if (type_name.size() > 2 &&
        type_name.front() == '[' && type_name.back() == ']')
        return llvm::PointerType::get(context, 0);
    auto it = struct_types.find(type_name);
    if (it != struct_types.end()) return it->second;
    auto eit = enum_variants.find(type_name);
    if (eit != enum_variants.end())
        return llvm::Type::getInt32Ty(context);
    return llvm::Type::getInt32Ty(context);
}

llvm::AllocaInst* Codegen::createEntryAlloca(llvm::Function* fn,
                                               const std::string& name,
                                               llvm::Type* type) {
    llvm::IRBuilder<> tmp(&fn->getEntryBlock(),
                           fn->getEntryBlock().begin());
    return tmp.CreateAlloca(type, nullptr, name);
}

std::string Codegen::convertBraceFormat(const std::string& fmt,
                                         const std::vector<llvm::Value*>& args) {
    std::string result;
    size_t arg_idx = 0, i = 0;
    while (i < fmt.size()) {
        if (fmt[i] == '{' && i+1 < fmt.size() && fmt[i+1] == '}') {
            if (arg_idx < args.size()) {
                llvm::Type* t = args[arg_idx]->getType();
                if (t->isIntegerTy(1))     result += "%d";
                else if (t->isIntegerTy()) result += "%d";
                else if (t->isFloatTy() ||
                         t->isDoubleTy())  result += "%f";
                else if (t->isPointerTy()) result += "%s";
                else                       result += "%d";
                arg_idx++;
            }
            i += 2;
        } else { result += fmt[i++]; }
    }
    return result;
}

std::string Codegen::getAsmClobbers() {
    std::string triple = llvm::sys::getDefaultTargetTriple();
    if (triple.find("aarch64") != std::string::npos ||
        triple.find("arm64")   != std::string::npos)
        return "~{x0},~{x1},~{x2},~{x3},~{x4},~{x5},~{x6},~{x7},"
               "~{x8},~{x9},~{x10},~{x11},~{x12},~{x13},~{x14},~{x15},"
               "~{x16},~{x17},~{x18},~{x19},~{x20},~{x21},~{x22},~{x23},"
               "~{memory}";
    if (triple.find("x86_64") != std::string::npos ||
        triple.find("amd64")  != std::string::npos)
        return "~{rax},~{rbx},~{rcx},~{rdx},~{rsi},~{rdi},"
               "~{r8},~{r9},~{r10},~{r11},~{r12},~{r13},~{r14},~{r15},"
               "~{memory}";
    if (triple.find("riscv64") != std::string::npos)
        return "~{a0},~{a1},~{a2},~{a3},~{a4},~{a5},~{a6},~{a7},"
               "~{t0},~{t1},~{t2},~{t3},~{t4},~{t5},~{t6},~{memory}";
    if (triple.find("riscv32") != std::string::npos)
        return "~{a0},~{a1},~{a2},~{a3},~{a4},~{a5},~{a6},~{a7},"
               "~{t0},~{t1},~{t2},~{t3},~{t4},~{t5},~{t6},~{memory}";
    return "~{memory}";
}

// ─── Top Level ──────────────────────────────────────────

void Codegen::generate(const Program& program,
                        const std::string& output_path) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    declarePrintf();

    for (const auto& c : program.constants) genConstDecl(c);
    for (const auto& e : program.extern_fns) genExternFn(e);
    for (const auto& s : program.structs) genStructDecl(s);
    for (const auto& e : program.enums) genEnumDecl(e);
    // Forward declare semua fungsi dulu
    for (const auto& fn : program.functions) {
        std::vector<llvm::Type*> param_types;
        for (const auto& p : fn.params)
            param_types.push_back(resolveType(p.type));

        llvm::Type* ret_type;
        if (!fn.return_types.empty()) {
            std::vector<llvm::Type*> ret_types;
            for (const auto& rt : fn.return_types)
                ret_types.push_back(resolveType(rt));
            ret_type = llvm::StructType::get(context, ret_types);
        } else if (fn.return_type == "void" || fn.return_type.empty()) {
            ret_type = llvm::Type::getVoidTy(context);
        } else {
            ret_type = resolveType(fn.return_type);
        }

        llvm::FunctionType* fn_type = llvm::FunctionType::get(
            ret_type, param_types, false
        );

        // Only create if not already declared
        if (!module->getFunction(fn.name)) {
            llvm::Function::Create(
                fn_type,
                llvm::Function::ExternalLinkage,
                fn.name,
                module.get()
            );
        }
    }

    // Generate function bodies
    for (const auto& fn : program.functions) {
        genFunction(fn);
    }

    std::string err;
    llvm::raw_string_ostream err_stream(err);
    if (llvm::verifyModule(*module, &err_stream))
        reporter.fatal(ErrorCode::E010_LLVM_VERIFY_FAILED,
                       "LLVM module verification failed: " + err);

    compileToObject(output_path, program.includes, program.cxx_includes);
}

llvm::Function* Codegen::declarePrintf() {
    if (auto* fn = module->getFunction("printf")) return fn;
    llvm::FunctionType* t = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context),
        { llvm::PointerType::get(context, 0) }, true
    );
    return llvm::Function::Create(t,
        llvm::Function::ExternalLinkage, "printf", module.get());
}

void Codegen::genConstDecl(const ConstDecl& c) {
    llvm::Type* type = resolveType(c.type);
    if (auto* ilit = dynamic_cast<const IntLitExpr*>(c.value.get())) {
        auto* gv = new llvm::GlobalVariable(
            *module, type, true,
            llvm::GlobalValue::InternalLinkage,
            llvm::ConstantInt::get(type, ilit->value), c.name
        );
        constants[c.name] = gv;
    } else if (auto* flit = dynamic_cast<const FloatLitExpr*>(c.value.get())) {
        auto* gv = new llvm::GlobalVariable(
            *module, type, true,
            llvm::GlobalValue::InternalLinkage,
            llvm::ConstantFP::get(type, flit->value), c.name
        );
        constants[c.name] = gv;
    }
}

void Codegen::genExternFn(const ExternFnDecl& e) {
    std::vector<llvm::Type*> param_types;
    for (const auto& p : e.params)
        param_types.push_back(resolveType(p.type));
    llvm::Type* ret = (e.return_type == "void" || e.return_type.empty())
        ? llvm::Type::getVoidTy(context)
        : resolveType(e.return_type);
    llvm::FunctionType* ft = llvm::FunctionType::get(ret, param_types, false);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                           e.name, module.get());
}

void Codegen::genStructDecl(const StructDecl& s) {
    std::vector<llvm::Type*> field_types;
    std::unordered_map<std::string, int> field_map;
    for (int i = 0; i < (int)s.fields.size(); i++) {
        field_types.push_back(resolveType(s.fields[i].type));
        field_map[s.fields[i].name] = i;
    }
    llvm::StructType* st = llvm::StructType::create(
        context, field_types, s.name
    );
    struct_types[s.name]  = st;
    struct_fields[s.name] = field_map;
}

void Codegen::genEnumDecl(const EnumDecl& e) {
    std::unordered_map<std::string, int> vm;
    for (int i = 0; i < (int)e.variants.size(); i++)
        vm[e.variants[i]] = i;
    enum_variants[e.name] = vm;
}

void Codegen::genFunction(const FnDecl& fn) {
    // Build return type
    llvm::Type* ret_type;
    if (!fn.return_types.empty()) {
        // Multi-return: use struct
        std::vector<llvm::Type*> ret_types;
        for (const auto& rt : fn.return_types)
            ret_types.push_back(resolveType(rt));
        ret_type = llvm::StructType::get(context, ret_types);
    } else if (fn.return_type == "void" || fn.return_type.empty()) {
        ret_type = llvm::Type::getVoidTy(context);
    } else {
        ret_type = resolveType(fn.return_type);
    }

    std::vector<llvm::Type*> param_types;
    for (const auto& p : fn.params)
        param_types.push_back(resolveType(p.type));

    llvm::FunctionType* fn_type = llvm::FunctionType::get(
        ret_type, param_types, false
    );
    // Get existing declaration or create new
    llvm::Function* llvm_fn = module->getFunction(fn.name);
    if (!llvm_fn) {
        llvm_fn = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage,
            fn.name, module.get()
        );
    }

    int i = 0;
    for (auto& arg : llvm_fn->args())
        arg.setName(fn.params[i++].name);

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(
        context, "entry", llvm_fn
    );
    builder->SetInsertPoint(entry);

    named_values.clear();
    var_struct_type.clear();
    array_elem_types.clear();

    i = 0;
    for (auto& arg : llvm_fn->args()) {
        llvm::AllocaInst* alloca = createEntryAlloca(
            llvm_fn, fn.params[i].name, param_types[i]
        );
        builder->CreateStore(&arg, alloca);
        named_values[fn.params[i].name] = alloca;
        i++;
    }

    for (size_t j = 0; j < fn.body.size(); j++) {
        const auto& stmt = fn.body[j];
        if (j == fn.body.size() - 1 &&
            ret_type != llvm::Type::getVoidTy(context) &&
            fn.return_types.empty()) {
            if (auto* es = dynamic_cast<const ExprStmt*>(stmt.get())) {
                llvm::Value* last_val = genExpr(*es->expr);
                if (!builder->GetInsertBlock()->getTerminator())
                    builder->CreateRet(last_val);
                continue;
            }
        }
        genStmt(*stmt);
    }

    if (!builder->GetInsertBlock()->getTerminator()) {
        if (ret_type == llvm::Type::getVoidTy(context))
            builder->CreateRetVoid();
        else
            builder->CreateRet(
                llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(context), 0)
            );
    }
}

// ─── Statements ─────────────────────────────────────────

void Codegen::genStmt(const Stmt& stmt) {
    if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt)) {
        genVarDecl(*s); return;
    }
    if (auto* s = dynamic_cast<const AssignStmt*>(&stmt)) {
        genAssign(*s); return;
    }
    if (auto* s = dynamic_cast<const ArrayAssignStmt*>(&stmt)) {
        genArrayAssign(*s); return;
    }
    if (auto* s = dynamic_cast<const FieldAssignStmt*>(&stmt)) {
        genFieldAssign(*s); return;
    }
    if (auto* s = dynamic_cast<const IfStmt*>(&stmt)) {
        genIf(*s); return;
    }
    if (auto* s = dynamic_cast<const WhileStmt*>(&stmt)) {
        genWhile(*s); return;
    }
    if (auto* s = dynamic_cast<const ForStmt*>(&stmt)) {
        genFor(*s); return;
    }
    if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt)) {
        genReturn(*s); return;
    }
    if (auto* s = dynamic_cast<const MultiReturnStmt*>(&stmt)) {
        genMultiReturn(*s); return;
    }
    if (auto* s = dynamic_cast<const TupleDestructStmt*>(&stmt)) {
        genTupleDestruct(*s); return;
    }
    if (auto* s = dynamic_cast<const AsmStmt*>(&stmt)) {
        genAsm(*s); return;
    }
    if (auto* s = dynamic_cast<const MatchStmt*>(&stmt)) {
        genMatch(*s); return;
    }
    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        if (break_target) builder->CreateBr(break_target);
        return;
    }
    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        if (continue_target) builder->CreateBr(continue_target);
        return;
    }
    if (dynamic_cast<const CxxStmt*>(&stmt)) {
        reporter.fatal(ErrorCode::E009_UNKNOWN_STATEMENT,
            "cxx{} block only supported in C++ transpiler mode");
        return;
    }
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt)) {
        genExprStmt(*s); return;
    }
    reporter.fatal(ErrorCode::E009_UNKNOWN_STATEMENT,
                   "unknown statement type");
}

void Codegen::genVarDecl(const VarDeclStmt& stmt) {
    if (!stmt.init) return;
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::Value* init_val = genExpr(*stmt.init);

    llvm::Type* type = init_val->getType();
    if (stmt.type != "auto" && stmt.type.front() != '[')
        type = resolveType(stmt.type);

    llvm::AllocaInst* alloca = createEntryAlloca(fn, stmt.name, type);
    builder->CreateStore(init_val, alloca);
    named_values[stmt.name] = alloca;

    if (auto* slit = dynamic_cast<const StructLitExpr*>(stmt.init.get()))
        var_struct_type[stmt.name] = slit->struct_name;
    else if (stmt.type != "auto" && struct_types.count(stmt.type))
        var_struct_type[stmt.name] = stmt.type;
}

void Codegen::genAssign(const AssignStmt& stmt) {
    auto it = named_values.find(stmt.name);
    if (it == named_values.end())
        reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                       "undefined variable '" + stmt.name + "'");
    builder->CreateStore(genExpr(*stmt.value), it->second);
}

void Codegen::genArrayAssign(const ArrayAssignStmt& stmt) {
    auto it = named_values.find(stmt.name);
    if (it == named_values.end())
        reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                       "undefined variable '" + stmt.name + "'");
    llvm::Value* arr_ptr = builder->CreateLoad(
        it->second->getAllocatedType(), it->second, stmt.name
    );
    llvm::Value* idx = genExpr(*stmt.index);
    llvm::Value* val = genExpr(*stmt.value);
    llvm::Type* elem_type = llvm::Type::getInt32Ty(context);
    auto et = array_elem_types.find(stmt.name);
    if (et != array_elem_types.end()) elem_type = et->second;
    llvm::Value* gep = builder->CreateGEP(elem_type, arr_ptr, idx, "arr_idx");
    builder->CreateStore(val, gep);
}

void Codegen::genFieldAssign(const FieldAssignStmt& stmt) {
    auto it = named_values.find(stmt.object);
    if (it == named_values.end())
        reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                       "undefined variable '" + stmt.object + "'");
    auto type_it = var_struct_type.find(stmt.object);
    if (type_it == var_struct_type.end())
        reporter.fatal(ErrorCode::E004_TYPE_MISMATCH,
                       "'" + stmt.object + "' is not a struct");
    const std::string& sname = type_it->second;
    auto field_it = struct_fields[sname].find(stmt.field);
    if (field_it == struct_fields[sname].end())
        reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                       "no field '" + stmt.field + "'");
    llvm::Value* gep = builder->CreateStructGEP(
        struct_types[sname], it->second, field_it->second, stmt.field
    );
    builder->CreateStore(genExpr(*stmt.value), gep);
}

void Codegen::genIf(const IfStmt& stmt) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::Value* cond = genExpr(*stmt.condition);
    if (cond->getType()->isIntegerTy() &&
        cond->getType()->getIntegerBitWidth() != 1)
        cond = builder->CreateICmpNE(
            cond, llvm::ConstantInt::get(cond->getType(), 0), "ifcond"
        );

    llvm::BasicBlock* then_bb  = llvm::BasicBlock::Create(context, "then", fn);
    llvm::BasicBlock* else_bb  = llvm::BasicBlock::Create(context, "else");
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "merge");

    builder->CreateCondBr(cond, then_bb, else_bb);

    builder->SetInsertPoint(then_bb);
    for (const auto& s : stmt.then_body) genStmt(*s);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(merge_bb);

    else_bb->insertInto(fn);
    builder->SetInsertPoint(else_bb);
    for (const auto& s : stmt.else_body) genStmt(*s);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(merge_bb);

    merge_bb->insertInto(fn);
    builder->SetInsertPoint(merge_bb);
}

void Codegen::genWhile(const WhileStmt& stmt) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(context, "while_cond", fn);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(context, "while_body", fn);
    llvm::BasicBlock* end_bb  = llvm::BasicBlock::Create(context, "while_end",  fn);

    builder->CreateBr(cond_bb);
    builder->SetInsertPoint(cond_bb);

    llvm::Value* cond = genExpr(*stmt.condition);
    if (cond->getType()->isIntegerTy() &&
        cond->getType()->getIntegerBitWidth() != 1)
        cond = builder->CreateICmpNE(
            cond, llvm::ConstantInt::get(cond->getType(), 0), "whilecond"
        );
    builder->CreateCondBr(cond, body_bb, end_bb);

    builder->SetInsertPoint(body_bb);
    break_target    = end_bb;
    continue_target = cond_bb;
    for (const auto& s : stmt.body) genStmt(*s);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(cond_bb);

    builder->SetInsertPoint(end_bb);
}

void Codegen::genFor(const ForStmt& stmt) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();

    llvm::Value* start_val = genExpr(*stmt.start);
    llvm::AllocaInst* loop_var = createEntryAlloca(
        fn, stmt.var, llvm::Type::getInt32Ty(context)
    );
    builder->CreateStore(start_val, loop_var);
    named_values[stmt.var] = loop_var;

    llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(context, "for_cond", fn);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(context, "for_body", fn);
    llvm::BasicBlock* incr_bb = llvm::BasicBlock::Create(context, "for_incr", fn);
    llvm::BasicBlock* end_bb  = llvm::BasicBlock::Create(context, "for_end",  fn);

    builder->CreateBr(cond_bb);
    builder->SetInsertPoint(cond_bb);

    llvm::Value* cur = builder->CreateLoad(
        llvm::Type::getInt32Ty(context), loop_var, stmt.var
    );
    llvm::Value* end_val = genExpr(*stmt.end);
    llvm::Value* cond = stmt.inclusive
        ? builder->CreateICmpSLE(cur, end_val, "for_cond")
        : builder->CreateICmpSLT(cur, end_val, "for_cond");
    builder->CreateCondBr(cond, body_bb, end_bb);

    builder->SetInsertPoint(body_bb);
    break_target    = end_bb;
    continue_target = incr_bb;
    for (const auto& s : stmt.body) genStmt(*s);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(incr_bb);

    builder->SetInsertPoint(incr_bb);
    llvm::Value* next = builder->CreateAdd(
        builder->CreateLoad(llvm::Type::getInt32Ty(context), loop_var, stmt.var),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1),
        "for_incr"
    );
    builder->CreateStore(next, loop_var);
    builder->CreateBr(cond_bb);

    builder->SetInsertPoint(end_bb);
}

void Codegen::genAsm(const AsmStmt& stmt) {
    llvm::FunctionType* asm_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context), false
    );
    llvm::InlineAsm* inline_asm = llvm::InlineAsm::get(
        asm_type, stmt.code, getAsmClobbers(), true
    );
    builder->CreateCall(inline_asm, {});
}

void Codegen::genReturn(const ReturnStmt& stmt) {
    builder->CreateRet(genExpr(*stmt.value));
}

void Codegen::genMultiReturn(const MultiReturnStmt& stmt) {
    // Pack into struct
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::Type* ret_type = fn->getReturnType();

    if (auto* st = llvm::dyn_cast<llvm::StructType>(ret_type)) {
        llvm::AllocaInst* alloca = createEntryAlloca(fn, "ret_tmp", st);
        for (int i = 0; i < (int)stmt.values.size(); i++) {
            llvm::Value* val = genExpr(*stmt.values[i]);
            llvm::Value* gep = builder->CreateStructGEP(st, alloca, i);
            builder->CreateStore(val, gep);
        }
        llvm::Value* result = builder->CreateLoad(st, alloca, "ret_val");
        builder->CreateRet(result);
    } else {
        // Fallback: return first value
        builder->CreateRet(genExpr(*stmt.values[0]));
    }
}

void Codegen::genTupleDestruct(const TupleDestructStmt& stmt) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::Value* tuple_val = genExpr(*stmt.value);

    if (auto* st = llvm::dyn_cast<llvm::StructType>(tuple_val->getType())) {
        // Alloca to store tuple
        llvm::AllocaInst* alloca = createEntryAlloca(fn, "tuple_tmp", st);
        builder->CreateStore(tuple_val, alloca);

        for (int i = 0; i < (int)stmt.names.size(); i++) {
            llvm::Value* gep = builder->CreateStructGEP(st, alloca, i);
            llvm::Value* val = builder->CreateLoad(
                st->getElementType(i), gep, stmt.names[i]
            );
            llvm::AllocaInst* var = createEntryAlloca(
                fn, stmt.names[i], st->getElementType(i)
            );
            builder->CreateStore(val, var);
            named_values[stmt.names[i]] = var;
        }
    }
}

void Codegen::genMatch(const MatchStmt& stmt) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::Value* match_val = genExpr(*stmt.value);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(
        context, "match_end", fn
    );

    for (const auto& arm : stmt.arms) {
        int variant_val = 0;
        auto eit = enum_variants.find(arm.enum_name);
        if (eit != enum_variants.end()) {
            auto vit = eit->second.find(arm.variant);
            if (vit != eit->second.end())
                variant_val = vit->second;
        }

        llvm::Value* arm_val = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context), variant_val
        );
        llvm::Value* cond = builder->CreateICmpEQ(
            match_val, arm_val, "match_cmp"
        );

        llvm::BasicBlock* arm_bb = llvm::BasicBlock::Create(
            context, "match_arm_" + arm.variant, fn
        );
        llvm::BasicBlock* next_bb = llvm::BasicBlock::Create(
            context, "match_next", fn
        );

        builder->CreateCondBr(cond, arm_bb, next_bb);
        builder->SetInsertPoint(arm_bb);
        for (const auto& s : arm.body) genStmt(*s);
        if (!builder->GetInsertBlock()->getTerminator())
            builder->CreateBr(end_bb);

        builder->SetInsertPoint(next_bb);
    }

    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(end_bb);
    builder->SetInsertPoint(end_bb);
}

void Codegen::genExprStmt(const ExprStmt& stmt) {
    genExpr(*stmt.expr);
}

// ─── Expressions ────────────────────────────────────────

llvm::Value* Codegen::genExpr(const Expr& expr) {
    if (auto* e = dynamic_cast<const IntLitExpr*>(&expr))
        return genIntLit(*e);
    if (auto* e = dynamic_cast<const FloatLitExpr*>(&expr))
        return genFloatLit(*e);
    if (auto* e = dynamic_cast<const StrLitExpr*>(&expr))
        return genStrLit(*e);
    if (auto* e = dynamic_cast<const BoolLitExpr*>(&expr))
        return genBoolLit(*e);
    if (auto* e = dynamic_cast<const IdentExpr*>(&expr))
        return genIdent(*e);
    if (auto* e = dynamic_cast<const BinOpExpr*>(&expr))
        return genBinOp(*e);
    if (auto* e = dynamic_cast<const CallExpr*>(&expr))
        return genCall(*e);
    if (auto* e = dynamic_cast<const PrintfExpr*>(&expr))
        return genPrintf(*e);
    if (auto* e = dynamic_cast<const StructLitExpr*>(&expr))
        return genStructLit(*e);
    if (auto* e = dynamic_cast<const FieldAccessExpr*>(&expr))
        return genFieldAccess(*e);
    if (auto* e = dynamic_cast<const EnumVariantExpr*>(&expr))
        return genEnumVariant(*e);
    if (auto* e = dynamic_cast<const ArrayLitExpr*>(&expr))
        return genArrayLit(*e);
    if (auto* e = dynamic_cast<const ArrayIndexExpr*>(&expr))
        return genArrayIndex(*e);
    if (auto* e = dynamic_cast<const CastExpr*>(&expr))
        return genCast(*e);
    if (auto* e = dynamic_cast<const RefExpr*>(&expr))
        return genRef(*e);
    if (auto* e = dynamic_cast<const MethodCallExpr*>(&expr))
        return genMethodCall(*e);
    if (auto* e = dynamic_cast<const ClosureExpr*>(&expr))
        return genClosure(*e);
    if (auto* e = dynamic_cast<const TupleExpr*>(&expr))
        return genTuple(*e);
    if (auto* e = dynamic_cast<const ResultExpr*>(&expr))
        return genResult(*e);
    if (auto* e = dynamic_cast<const ErrorPropExpr*>(&expr))
        return genErrorProp(*e);
    if (auto* e = dynamic_cast<const AnyExpr*>(&expr))
        return genExpr(*e->value);
    reporter.fatal(ErrorCode::E009_UNKNOWN_STATEMENT,
                   "unknown expression type");
}

llvm::Value* Codegen::genIntLit(const IntLitExpr& expr) {
    return llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), expr.value
    );
}

llvm::Value* Codegen::genFloatLit(const FloatLitExpr& expr) {
    return llvm::ConstantFP::get(
        llvm::Type::getFloatTy(context), expr.value
    );
}

llvm::Value* Codegen::genStrLit(const StrLitExpr& expr) {
    return builder->CreateGlobalString(expr.value, ".str");
}

llvm::Value* Codegen::genBoolLit(const BoolLitExpr& expr) {
    return llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(context), expr.value ? 1 : 0
    );
}

llvm::Value* Codegen::genIdent(const IdentExpr& expr) {
    auto cit = constants.find(expr.name);
    if (cit != constants.end()) {
        return builder->CreateLoad(
            llvm::cast<llvm::GlobalVariable>(cit->second)->getValueType(),
            cit->second, expr.name
        );
    }
    auto it = named_values.find(expr.name);
    if (it == named_values.end())
        reporter.error(ErrorCode::E001_UNDEFINED_VAR,
                       "undefined variable '" + expr.name + "'",
                       expr.line, expr.col);
    return builder->CreateLoad(
        it->second->getAllocatedType(), it->second, expr.name
    );
}

llvm::Value* Codegen::genStructLit(const StructLitExpr& expr) {
    auto it = struct_types.find(expr.struct_name);
    if (it == struct_types.end())
        reporter.fatal(ErrorCode::E004_TYPE_MISMATCH,
                       "undefined struct '" + expr.struct_name + "'");

    llvm::StructType* st = it->second;
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::AllocaInst* alloca = createEntryAlloca(
        fn, expr.struct_name + "_tmp", st
    );

    auto& fm = struct_fields[expr.struct_name];
    for (const auto& f : expr.fields) {
        auto fidx = fm.find(f.first);
        if (fidx == fm.end())
            reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                           "no field '" + f.first + "'");
        llvm::Value* gep = builder->CreateStructGEP(
            st, alloca, fidx->second, f.first
        );
        builder->CreateStore(genExpr(*f.second), gep);
    }
    return builder->CreateLoad(st, alloca, "struct_val");
}

llvm::Value* Codegen::genFieldAccess(const FieldAccessExpr& expr) {
    const IdentExpr* ident = dynamic_cast<const IdentExpr*>(expr.object.get());
    if (!ident)
        reporter.fatal(ErrorCode::E004_TYPE_MISMATCH,
                       "field access only on named variables");

    auto it = named_values.find(ident->name);
    if (it == named_values.end())
        reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                       "undefined variable '" + ident->name + "'");

    auto type_it = var_struct_type.find(ident->name);
    if (type_it == var_struct_type.end())
        reporter.fatal(ErrorCode::E004_TYPE_MISMATCH,
                       "'" + ident->name + "' is not a struct");

    const std::string& sname = type_it->second;
    auto field_it = struct_fields[sname].find(expr.field);
    if (field_it == struct_fields[sname].end())
        reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                       "no field '" + expr.field + "'");

    llvm::Value* gep = builder->CreateStructGEP(
        struct_types[sname], it->second, field_it->second, expr.field
    );
    return builder->CreateLoad(
        struct_types[sname]->getElementType(field_it->second),
        gep, expr.field
    );
}

llvm::Value* Codegen::genEnumVariant(const EnumVariantExpr& expr) {
    auto eit = enum_variants.find(expr.enum_name);
    if (eit == enum_variants.end())
        reporter.fatal(ErrorCode::E004_TYPE_MISMATCH,
                       "undefined enum '" + expr.enum_name + "'");
    auto vit = eit->second.find(expr.variant);
    if (vit == eit->second.end())
        reporter.fatal(ErrorCode::E001_UNDEFINED_VAR,
                       "undefined variant '" + expr.variant + "'");
    return llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), vit->second
    );
}

llvm::Value* Codegen::genArrayLit(const ArrayLitExpr& expr) {
    if (expr.elements.empty())
        reporter.fatal(ErrorCode::E004_TYPE_MISMATCH,
                       "empty array literal");

    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    std::vector<llvm::Value*> vals;
    for (const auto& e : expr.elements)
        vals.push_back(genExpr(*e));

    llvm::Type* elem_type = vals[0]->getType();
    int n = (int)vals.size();
    llvm::ArrayType* arr_type = llvm::ArrayType::get(elem_type, n);
    llvm::AllocaInst* alloca = createEntryAlloca(fn, "arr_tmp", arr_type);

    for (int i = 0; i < n; i++) {
        llvm::Value* gep = builder->CreateGEP(
            arr_type, alloca,
            { llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
              llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i) },
            "arr_elem"
        );
        builder->CreateStore(vals[i], gep);
    }

    return builder->CreateGEP(
        arr_type, alloca,
        { llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0) },
        "arr_ptr"
    );
}

llvm::Value* Codegen::genArrayIndex(const ArrayIndexExpr& expr) {
    llvm::Value* arr_ptr = genExpr(*expr.array);
    llvm::Value* idx     = genExpr(*expr.index);
    llvm::Type* elem_type = llvm::Type::getInt32Ty(context);
    llvm::Value* gep = builder->CreateGEP(
        elem_type, arr_ptr, idx, "arr_idx"
    );
    return builder->CreateLoad(elem_type, gep, "arr_val");
}

llvm::Value* Codegen::genCast(const CastExpr& expr) {
    llvm::Value* val = genExpr(*expr.value);
    llvm::Type*  dst = resolveType(expr.target_type);

    if (val->getType()->isIntegerTy() &&
        (dst->isFloatTy() || dst->isDoubleTy()))
        return builder->CreateSIToFP(val, dst, "cast");
    if ((val->getType()->isFloatTy() ||
         val->getType()->isDoubleTy()) &&
        dst->isIntegerTy())
        return builder->CreateFPToSI(val, dst, "cast");
    if (val->getType()->isIntegerTy() && dst->isIntegerTy())
        return builder->CreateIntCast(val, dst, true, "cast");
    return val;
}

llvm::Value* Codegen::genRef(const RefExpr& expr) {
    if (auto* ident = dynamic_cast<const IdentExpr*>(expr.value.get())) {
        auto it = named_values.find(ident->name);
        if (it != named_values.end())
            return it->second;
    }
    return genExpr(*expr.value);
}

llvm::Value* Codegen::genMethodCall(const MethodCallExpr& expr) {
    reporter.fatal(ErrorCode::E002_UNDEFINED_FN,
                   "method '" + expr.method +
                   "' not yet supported in LLVM mode");
}

llvm::Value* Codegen::genClosure(const ClosureExpr& expr) {
    // Generate closure as anonymous function
    std::string name = "__closure_" + std::to_string(closure_counter++);

    std::vector<llvm::Type*> param_types;
    for (const auto& p : expr.params)
        param_types.push_back(resolveType(p.first));

    llvm::Type* ret_type = (expr.return_type == "void" ||
                             expr.return_type.empty())
        ? llvm::Type::getVoidTy(context)
        : resolveType(expr.return_type);

    llvm::FunctionType* fn_type = llvm::FunctionType::get(
        ret_type, param_types, false
    );
    llvm::Function* fn = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage,
        name, module.get()
    );

    // Save current insert point
    llvm::BasicBlock* saved_bb = builder->GetInsertBlock();
    auto saved_named = named_values;

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(
        context, "entry", fn
    );
    builder->SetInsertPoint(entry);

    named_values.clear();
    int i = 0;
    for (auto& arg : fn->args()) {
        arg.setName(expr.params[i].second);
        llvm::AllocaInst* alloca = createEntryAlloca(
            fn, expr.params[i].second, param_types[i]
        );
        builder->CreateStore(&arg, alloca);
        named_values[expr.params[i].second] = alloca;
        i++;
    }

    for (size_t j = 0; j < expr.body.size(); j++) {
        const auto& stmt = expr.body[j];
        if (j == expr.body.size() - 1 &&
            ret_type != llvm::Type::getVoidTy(context)) {
            if (auto* es = dynamic_cast<const ExprStmt*>(stmt.get())) {
                llvm::Value* v = genExpr(*es->expr);
                if (!builder->GetInsertBlock()->getTerminator())
                    builder->CreateRet(v);
                continue;
            }
        }
        genStmt(*stmt);
    }

    if (!builder->GetInsertBlock()->getTerminator()) {
        if (ret_type == llvm::Type::getVoidTy(context))
            builder->CreateRetVoid();
        else
            builder->CreateRet(
                llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(context), 0)
            );
    }

    // Restore
    named_values = std::move(saved_named);
    builder->SetInsertPoint(saved_bb);

    // Return function pointer
    return fn;
}

llvm::Value* Codegen::genTuple(const TupleExpr& expr) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();

    std::vector<llvm::Value*> vals;
    std::vector<llvm::Type*> types;
    for (const auto& e : expr.elements) {
        llvm::Value* v = genExpr(*e);
        vals.push_back(v);
        types.push_back(v->getType());
    }

    llvm::StructType* tuple_type = llvm::StructType::get(context, types);
    llvm::AllocaInst* alloca = createEntryAlloca(fn, "tuple_tmp", tuple_type);

    for (int i = 0; i < (int)vals.size(); i++) {
        llvm::Value* gep = builder->CreateStructGEP(
            tuple_type, alloca, i
        );
        builder->CreateStore(vals[i], gep);
    }

    return builder->CreateLoad(tuple_type, alloca, "tuple_val");
}

llvm::Value* Codegen::genResult(const ResultExpr& expr) {
    // Simple implementation: Ok = value, Err = 0
    return genExpr(*expr.value);
}

llvm::Value* Codegen::genErrorProp(const ErrorPropExpr& expr) {
    // Simple: just return the value (full impl needs Result type)
    return genExpr(*expr.value);
}

llvm::Value* Codegen::genBinOp(const BinOpExpr& expr) {
    if (!expr.left) {
        llvm::Value* right = genExpr(*expr.right);
        if (expr.op == "-") {
            if (right->getType()->isFloatTy() ||
                right->getType()->isDoubleTy())
                return builder->CreateFNeg(right, "fneg");
            return builder->CreateNeg(right, "neg");
        }
        if (expr.op == "!")
            return builder->CreateNot(right, "not");
        return right;
    }

    llvm::Value* left  = genExpr(*expr.left);
    llvm::Value* right = genExpr(*expr.right);
    bool is_float = left->getType()->isFloatTy()  ||
                    right->getType()->isFloatTy()  ||
                    left->getType()->isDoubleTy()  ||
                    right->getType()->isDoubleTy();

    if (expr.op == "+") return is_float
        ? builder->CreateFAdd(left, right, "fadd")
        : builder->CreateAdd(left, right, "add");
    if (expr.op == "-") return is_float
        ? builder->CreateFSub(left, right, "fsub")
        : builder->CreateSub(left, right, "sub");
    if (expr.op == "*") return is_float
        ? builder->CreateFMul(left, right, "fmul")
        : builder->CreateMul(left, right, "mul");
    if (expr.op == "/") return is_float
        ? builder->CreateFDiv(left, right, "fdiv")
        : builder->CreateSDiv(left, right, "div");
    if (expr.op == "%")
        return builder->CreateSRem(left, right, "rem");
    if (expr.op == "==") return is_float
        ? builder->CreateFCmpOEQ(left, right, "feq")
        : builder->CreateICmpEQ(left, right, "eq");
    if (expr.op == "!=") return is_float
        ? builder->CreateFCmpONE(left, right, "fne")
        : builder->CreateICmpNE(left, right, "ne");
    if (expr.op == "<") return is_float
        ? builder->CreateFCmpOLT(left, right, "flt")
        : builder->CreateICmpSLT(left, right, "lt");
    if (expr.op == ">") return is_float
        ? builder->CreateFCmpOGT(left, right, "fgt")
        : builder->CreateICmpSGT(left, right, "gt");
    if (expr.op == "<=") return is_float
        ? builder->CreateFCmpOLE(left, right, "fle")
        : builder->CreateICmpSLE(left, right, "le");
    if (expr.op == ">=") return is_float
        ? builder->CreateFCmpOGE(left, right, "fge")
        : builder->CreateICmpSGE(left, right, "ge");
    if (expr.op == "&&")
        return builder->CreateAnd(left, right, "and");
    if (expr.op == "||")
        return builder->CreateOr(left, right, "or");

    reporter.fatal(ErrorCode::E008_UNKNOWN_OPERATOR,
                   "unknown operator '" + expr.op + "'");
}

llvm::Value* Codegen::genCall(const CallExpr& expr) {
    std::vector<llvm::Value*> arg_vals;
    for (const auto& arg : expr.args)
        arg_vals.push_back(genExpr(*arg));

    llvm::Function* fn = module->getFunction(expr.callee);

    if (!fn) {
        std::vector<llvm::Type*> param_types;
        for (auto* v : arg_vals)
            param_types.push_back(v->getType());

        llvm::Type* ret_type = llvm::Type::getInt32Ty(context);
        if (expr.callee == "sqrt" || expr.callee == "sin"  ||
            expr.callee == "cos"  || expr.callee == "tan"  ||
            expr.callee == "log"  || expr.callee == "exp"  ||
            expr.callee == "pow"  || expr.callee == "fabs")
            ret_type = llvm::Type::getDoubleTy(context);
        else if (expr.callee == "malloc" ||
                 expr.callee == "calloc" ||
                 expr.callee == "realloc")
            ret_type = llvm::PointerType::get(context, 0);

        bool is_variadic = (expr.callee == "printf"  ||
                            expr.callee == "scanf"   ||
                            expr.callee == "sprintf" ||
                            expr.callee == "fprintf");

        llvm::FunctionType* ft = llvm::FunctionType::get(
            ret_type, param_types, is_variadic
        );
        fn = llvm::Function::Create(ft,
            llvm::Function::ExternalLinkage,
            expr.callee, module.get());
    }

    return builder->CreateCall(fn, arg_vals, "calltmp");
}

llvm::Value* Codegen::genPrintf(const PrintfExpr& expr) {
    llvm::Function* printf_fn = module->getFunction("printf");
    if (!printf_fn) printf_fn = declarePrintf();

    std::vector<llvm::Value*> arg_vals;
    for (const auto& arg : expr.args)
        arg_vals.push_back(genExpr(*arg));

    std::string final_fmt = expr.is_brace_style
        ? convertBraceFormat(expr.format, arg_vals)
        : expr.format;

    if (final_fmt.empty() || final_fmt.back() != '\n')
        final_fmt += '\n';

    llvm::Value* fmt_str = builder->CreateGlobalString(final_fmt, ".fmt");
    std::vector<llvm::Value*> call_args = { fmt_str };
    for (auto* v : arg_vals) call_args.push_back(v);

    return builder->CreateCall(printf_fn, call_args, "printf_call");
}

// ─── Output ─────────────────────────────────────────────
// DEBUG: print LLVM IR

void Codegen::compileToObject(const std::string& output_path,
                               const std::vector<std::string>& c_includes,
                               const std::vector<std::string>& cxx_includes) {
    auto triple_str = llvm::sys::getDefaultTargetTriple();
    llvm::Triple triple(triple_str);
    module->setTargetTriple(triple);

    std::string error;
    const llvm::Target* target =
        llvm::TargetRegistry::lookupTarget(triple_str, error);
    if (!target)
        reporter.fatal(ErrorCode::E010_LLVM_VERIFY_FAILED,
                       "target not found: " + error);

    llvm::TargetOptions opt;
    auto* tm = target->createTargetMachine(
        triple, "generic", "", opt,
        llvm::Reloc::PIC_, llvm::CodeModel::Small,
        llvm::CodeGenOptLevel::Default
    );
    module->setDataLayout(tm->createDataLayout());

    std::string obj_path = output_path + ".o";
    std::error_code ec;
    llvm::raw_fd_ostream dest(obj_path, ec, llvm::sys::fs::OF_None);
    if (ec)
        reporter.fatal(ErrorCode::E006_CANNOT_OPEN_FILE,
                       "cannot open output: " + ec.message());

    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr,
            llvm::CodeGenFileType::ObjectFile))
        reporter.fatal(ErrorCode::E010_LLVM_VERIFY_FAILED,
                       "cannot emit object file");

    pass.run(*module);
    dest.flush();

    bool has_cxx = !cxx_includes.empty();
    std::string link_cmd = has_cxx ? "clang++ " : "clang ";
    link_cmd += obj_path + " -o " + output_path;

    for (const auto& inc : c_includes)
        if (inc.find("math") != std::string::npos)
            link_cmd += " -lm";

    int ret = system(link_cmd.c_str());
    std::remove(obj_path.c_str());

    if (ret != 0)
        reporter.fatal(ErrorCode::E010_LLVM_VERIFY_FAILED,
                       "linking failed");
}
