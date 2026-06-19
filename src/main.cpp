#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "transpiler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

// ============================================================================
// Flux Language Runtime - Entry Point
//
// Usage:
//   flux run <file.flux>                    Run a Flux source file (interpreted)
//   flux <file.flux>                        Same as above
//   flux compile <file.flux>                Compile to native binary (AOT)
//   flux compile <file.flux> -o <output>    Compile with custom output name
//   flux compile <file.flux> --fast          Compile fast (-O0)
//   flux compile <file.flux> --release       Max optimization (-O3)
//   flux compile <file.flux> --size          Minimize size (-Os)
//   flux                                    Interactive REPL
// ============================================================================

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file '" << path << "'" << std::endl;
        std::exit(1);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    return content;
}

static int runFile(const std::string& path) {
    std::string source = readFile(path);

    // Lex
    Lexer lexer(source, path);
    auto tokens = lexer.tokenize();

    if (lexer.hasErrors()) {
        for (auto& err : lexer.getErrors()) {
            std::cerr << err << std::endl;
        }
        return 1;
    }

    // Parse
    Parser parser(tokens, path);
    auto program = parser.parse();

    if (parser.hasErrors()) {
        for (auto& err : parser.getErrors()) {
            std::cerr << err << std::endl;
        }
        return 1;
    }

    // Interpret
    Interpreter interpreter;
    try {
        interpreter.execute(program);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (interpreter.hasErrors()) {
        for (auto& err : interpreter.getErrors()) {
            std::cerr << err << std::endl;
        }
        return 1;
    }

    return 0;
}

static void runRepl() {
    std::cout << "Flux v0.1 Interactive Shell" << std::endl;
    std::cout << "Type expressions or statements. Type 'exit' to quit." << std::endl;
    std::cout << std::endl;

    Interpreter interpreter;
    std::string line;

    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        // Auto-add semicolon if missing for simple expressions
        std::string source = line;
        if (source.back() != ';' && source.back() != '}') {
            source += ";";
        }

        Lexer lexer(source, "<repl>");
        auto tokens = lexer.tokenize();

        if (lexer.hasErrors()) {
            for (auto& err : lexer.getErrors()) {
                std::cerr << err << std::endl;
            }
            continue;
        }

        Parser parser(tokens, "<repl>");
        auto program = parser.parse();

        if (parser.hasErrors()) {
            for (auto& err : parser.getErrors()) {
                std::cerr << err << std::endl;
            }
            continue;
        }

        try {
            // For REPL, execute directly (don't look for main)
            auto prog = std::dynamic_pointer_cast<ProgramNode>(program);
            if (prog) {
                for (auto& decl : prog->declarations) {
                    Value result = interpreter.eval(decl);
                    // Print non-nil results for expression statements
                    if (decl->nodeType == NodeType::EXPRESSION_STMT &&
                        result.type != ValueType::NIL) {
                        std::cout << result.toString() << std::endl;
                    }
                }
            }
        } catch (const ReturnSignal& ret) {
            std::cout << ret.value.toString() << std::endl;
        } catch (const PanicSignal& panic) {
            std::cerr << "PANIC: " << panic.message << std::endl;
        } catch (const FluxException& e) {
            std::cerr << "[" << e.errorType << "] " << e.message << std::endl;
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    std::cout << std::endl << "Goodbye." << std::endl;
}

static void printUsage() {
    std::cout << "Flux Programming Language v0.1" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  flux run <file.flux>                Run interpreted" << std::endl;
    std::cout << "  flux <file.flux>                    Run interpreted" << std::endl;
    std::cout << "  flux compile <file.flux>            Compile to native binary" << std::endl;
    std::cout << "  flux compile <file.flux> -o <name>  Custom output binary name" << std::endl;
    std::cout << "  flux compile <file.flux> --fast     Fast compile (-O0)" << std::endl;
    std::cout << "  flux compile <file.flux> --release  Max optimization (-O3)" << std::endl;
    std::cout << "  flux compile <file.flux> --size     Minimize binary size (-Os)" << std::endl;
    std::cout << "  flux compile <file.flux> --dev      Save generated C++ source" << std::endl;
    std::cout << "  flux                                Start interactive REPL" << std::endl;
    std::cout << std::endl;
}

static int compileFile(int argc, char* argv[], int startIdx) {
    if (startIdx >= argc) {
        std::cerr << "Error: 'flux compile' requires a filename." << std::endl;
        return 1;
    }

    std::string inputFile = argv[startIdx];
    std::string outputFile;
    std::string optLevel = "-O2";
    bool devMode = false;

    // Parse additional flags
    for (int i = startIdx + 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "--fast") {
            optLevel = "-O0";
        } else if (arg == "--release") {
            optLevel = "-O3";
        } else if (arg == "--size") {
            optLevel = "-Os";
        } else if (arg == "--dev") {
            devMode = true;
        }
    }

    // Default output name: strip extension
    if (outputFile.empty()) {
        outputFile = inputFile;
        auto dot = outputFile.rfind('.');
        if (dot != std::string::npos) {
            outputFile = outputFile.substr(0, dot);
        }
    }

    std::string source = readFile(inputFile);
    Transpiler transpiler;
    // Set the base directory for resolving relative imports
    std::filesystem::path srcPath = std::filesystem::absolute(inputFile);
    transpiler.baseDir = srcPath.parent_path().string();
    bool success = transpiler.compile(source, outputFile, optLevel, devMode);
    return success ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        // No arguments: start REPL
        runRepl();
        return 0;
    }

    std::string firstArg = argv[1];

    if (firstArg == "--help" || firstArg == "-h") {
        printUsage();
        return 0;
    }

    if (firstArg == "run") {
        if (argc < 3) {
            std::cerr << "Error: 'flux run' requires a filename." << std::endl;
            printUsage();
            return 1;
        }
        return runFile(argv[2]);
    }

    if (firstArg == "compile") {
        return compileFile(argc, argv, 2);
    }

    // Assume the argument is a filename
    return runFile(firstArg);
}
