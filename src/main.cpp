#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>

#include "../include/error.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/transpiler.h"

#include "llvm/TargetParser/Host.h"

// ─── File helpers ───────────────────────────────────────

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("cannot open file: " + path);
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static std::string dirOf(const std::string& path) {
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.bada> -o <output>\n";
    std::cerr << "Example: bada hello.bada -o hello\n";
}

// ─── Import Resolver ────────────────────────────────────

static Program parseFile(const std::string& path,
                          ErrorReporter& reporter,
                          std::vector<std::string>& c_incs,
                          std::vector<std::string>& cxx_incs,
                          std::set<std::string>& visited,
                          const std::string& module_prefix = "");

static Program parseFile(const std::string& path,
                          ErrorReporter& reporter,
                          std::vector<std::string>& c_incs,
                          std::vector<std::string>& cxx_incs,
                          std::set<std::string>& visited,
                          const std::string& module_prefix) {
    if (visited.count(path)) return Program{};
    visited.insert(path);

    std::string source;
    try {
        source = readFile(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return Program{};
    }

    ErrorReporter file_reporter(path, source);

    Lexer lexer(source, path, file_reporter);
    std::vector<Token> tokens;
    try {
        tokens = lexer.tokenize();
    } catch (const BadaError&) {
        return Program{};
    }

    // Collect includes
    for (const auto& inc : lexer.c_includes)   c_incs.push_back(inc);
    for (const auto& inc : lexer.cxx_includes) cxx_incs.push_back(inc);

    Parser parser(std::move(tokens), file_reporter);
    Program program;
    try {
        program = parser.parse();
    } catch (const BadaError&) {
        return Program{};
    }

    program.includes     = lexer.c_includes;
    program.cxx_includes = lexer.cxx_includes;

    // Prefix functions with module name
    if (!module_prefix.empty()) {
        for (auto& fn : program.functions) {
            if (fn.name != "main")
                fn.name = module_prefix + "_" + fn.name;
        }
    }

    // Resolve imports recursively
    std::string dir = dirOf(path);
    for (const auto& imp : program.imports) {
        std::vector<std::string> search_paths = {
            dir + "/" + imp.module_name + ".bada",
            std::string(getenv("PREFIX") ? getenv("PREFIX") : "") +
                "/share/bada/lib/" + imp.module_name + ".bada",
        };

        bool found = false;
        for (const auto& search : search_paths) {
            if (fileExists(search)) {
                std::cerr << "[bada] importing: " << search << "\n";
                Program imported = parseFile(
                    search, reporter, c_incs, cxx_incs,
                    visited, imp.module_name
                );

                // Merge into current program
                for (auto& fn : imported.functions) {
                    if (fn.name == "main") continue;
                    program.functions.push_back(std::move(fn));
                }
                for (auto& s : imported.structs)
                    program.structs.push_back(std::move(s));
                for (auto& e : imported.enums)
                    program.enums.push_back(std::move(e));
                for (auto& c : imported.constants)
                    program.constants.push_back(std::move(c));
                for (auto& e : imported.extern_fns)
                    program.extern_fns.push_back(std::move(e));

                // Merge includes
                for (const auto& inc : imported.includes)
                    c_incs.push_back(inc);
                for (const auto& inc : imported.cxx_includes)
                    cxx_incs.push_back(inc);

                found = true;
                break;
            }
        }

        if (!found) {
            std::cerr << "error[E007]: module not found: '"
                      << imp.module_name << "'\n";
            std::cerr << "  searched:\n";
            for (const auto& s : search_paths)
                std::cerr << "    - " << s << "\n";
        }
    }

    return program;
}

// ─── Main ───────────────────────────────────────────────

int main(int argc, char* argv[]) {

    // ─── Version & Help ─────────────────────────────────
    if (argc == 2) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::cout << "bada 1.2.0\n";
            std::cout << "Bada-Lang Compiler\n";
            std::cout << "Target: "
                      << llvm::sys::getDefaultTargetTriple() << "\n";
            std::cout << "Modes:  LLVM IR | C++ Transpiler\n";
            std::cout << "Arch:   ARM64 | x86_64 | RISC-V\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "Bada-Lang Compiler v1.2.0\n\n";
            std::cout << "Usage:\n";
            std::cout << "  bada <input.bada> -o <output>\n\n";
            std::cout << "Options:\n";
            std::cout << "  -o <file>     Output binary\n";
            std::cout << "  --version     Show version info\n";
            std::cout << "  --help        Show this help\n\n";
            std::cout << "Examples:\n";
            std::cout << "  bada hello.bada -o hello\n";
            std::cout << "  ./hello\n\n";
            std::cout << "Import system:\n";
            std::cout << "  import math;           same directory\n";
            std::cout << "  import math as m;      with alias\n\n";
            std::cout << "Modes:\n";
            std::cout << "  LLVM IR mode      (no #include)\n";
            std::cout << "  C++ Transpiler    (with #include)\n";
            return 0;
        }
    }

    // ─── Argument Parsing ───────────────────────────────
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string input_path;
    std::string output_path;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o") {
            if (i + 1 < argc) output_path = argv[++i];
            else {
                std::cerr << "Error: -o requires an argument\n";
                return 1;
            }
        } else {
            input_path = arg;
        }
    }

    if (input_path.empty()) {
        std::cerr << "Error: no input file\n";
        printUsage(argv[0]); return 1;
    }
    if (output_path.empty()) {
        std::cerr << "Error: no output file\n";
        printUsage(argv[0]); return 1;
    }

    // ─── Read & Parse with import resolution ────────────
    std::string source;
    try {
        source = readFile(input_path);
    } catch (const std::exception& e) {
        std::cerr << "error[E006]: " << e.what() << "\n";
        return 1;
    }

    ErrorReporter reporter(input_path, source);
    std::vector<std::string> c_incs, cxx_incs;
    std::set<std::string> visited;

    Program program;
    try {
        program = parseFile(input_path, reporter,
                            c_incs, cxx_incs, visited, "");
    } catch (const BadaError&) {
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    program.includes     = c_incs;
    program.cxx_includes = cxx_incs;


    // ─── Auto-detect Pipeline ───────────────────────────
    bool has_includes = !c_incs.empty()   ||
                        !cxx_incs.empty() ||
                        !program.uses.empty();

    if (has_includes) {
        std::cerr << "[bada] C++ transpiler mode\n";
        try {
            Transpiler transpiler(
                program, input_path,
                c_incs, cxx_incs, reporter
            );
            transpiler.compile(output_path);
        } catch (const BadaError&) {
            return 1;
        }
    } else {
        std::cerr << "[bada] LLVM IR mode\n";
        try {
            Codegen codegen(reporter);
            codegen.generate(program, output_path);
        } catch (const BadaError&) {
            return 1;
        }
    }

    std::cout << "Compiled: " << input_path
              << " -> " << output_path << "\n";
    return 0;
}
