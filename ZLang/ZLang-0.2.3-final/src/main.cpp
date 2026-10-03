#include "zlang/CodeGen.h"
#include "zlang/Lexer.h"
#include "zlang/Parser.h"
#include "zlang/Process.h"
#include "zlang/Semantic.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
constexpr const char* Version = "Z Language Compiler 0.2.3";

enum ExitCode { Success = 0, SourceError = 1, CliError = 2, ToolchainError = 3, InternalError = 4 };

void printHelp()
{
    std::cout << Version << "\n\n"
              << "Usage:\n"
              << "  zlang source.z\n"
              << "  zlang build source.z [-o program.exe]\n"
              << "  zlang run source.z\n"
              << "\nOptions:\n"
              << "  -o <file>           Output executable\n"
              << "  --emit-cpp[=file]   Keep generated C++\n"
              << "  --cxx <path>        Select C++ compiler\n"
              << "  --quiet             Hide progress\n"
              << "  --help              Show help\n"
              << "  --version           Show version\n";
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isMsvc(const std::filesystem::path& compiler)
{
    const std::string name = lower(compiler.filename().string());
    return name == "cl.exe" || name == "cl";
}

std::filesystem::path normalizePath(const std::filesystem::path& path)
{
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    if (error) absolute = path;
    auto canonical = std::filesystem::weakly_canonical(absolute, error);
    return error ? absolute.lexically_normal() : canonical;
}

void loadSourceTree(const std::filesystem::path& requestedPath,
                    const std::string& moduleName,
                    bool isRoot,
                    const zlang::SourceSpan& importSite,
                    zlang::Diagnostics& diagnostics,
                    zlang::Program& combined,
                    std::unordered_set<std::string>& loading,
                    std::unordered_set<std::string>& loaded,
                    std::unordered_map<std::string, std::string>& modulePaths)
{
    const auto path = normalizePath(requestedPath);
    const std::string pathKey = path.generic_string();
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(path, filesystemError)) {
        diagnostics.error(importSite,
                          isRoot ? "cannot open source: " + path.string()
                                 : "cannot find imported module '" + moduleName + "' at " +
                                       path.string());
        return;
    }

    if (!moduleName.empty()) {
        const auto existing = modulePaths.find(moduleName);
        if (existing != modulePaths.end() && existing->second != pathKey) {
            diagnostics.error(importSite,
                              "module name '" + moduleName + "' refers to more than one file");
            return;
        }
        modulePaths[moduleName] = pathKey;
    }

    if (loading.count(pathKey) != 0) {
        diagnostics.error(importSite, "cyclic import detected for module '" + moduleName + "'");
        return;
    }
    if (loaded.count(pathKey) != 0) return;

    std::ifstream sourceFile(path, std::ios::binary);
    if (!sourceFile) {
        diagnostics.error(importSite, "cannot read source file: " + path.string());
        return;
    }
    std::ostringstream sourceBuffer;
    sourceBuffer << sourceFile.rdbuf();
    const std::string source = sourceBuffer.str();
    diagnostics.addSource(path.string(), source);

    const std::size_t diagnosticCount = diagnostics.all().size();
    zlang::Lexer lexer(path.string(), source, diagnostics);
    auto tokens = lexer.scan();
    zlang::Parser parser(std::move(tokens), diagnostics);
    auto parsed = parser.parse();
    if (diagnostics.all().size() != diagnosticCount) return;

    loading.insert(pathKey);
    for (const auto& import : parsed.imports) {
        auto importPath = path.parent_path() / (import.module + ".z");
        loadSourceTree(importPath,
                       import.module,
                       false,
                       import.span,
                       diagnostics,
                       combined,
                       loading,
                       loaded,
                       modulePaths);
    }

    for (auto& function : parsed.functions) {
        function->module = moduleName;
        if (!isRoot && function->name == "main") {
            diagnostics.error(function->span,
                              "imported modules cannot define main(); define it in the entry file");
            continue;
        }
        combined.functions.push_back(std::move(function));
    }

    loading.erase(pathKey);
    loaded.insert(pathKey);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc < 2) {
            printHelp();
            return CliError;
        }

        const std::string first = argv[1];
        if (first == "--help" || first == "-h") {
            printHelp();
            return Success;
        }
        if (first == "--version") {
            std::cout << Version << '\n';
            return Success;
        }

        bool runAfterBuild = first == "run";
        int index = (first == "build" || first == "run") ? 2 : 1;
        if (index >= argc) {
            std::cerr << "error: missing source file\n";
            return CliError;
        }

        const std::filesystem::path sourcePath = argv[index++];
        std::filesystem::path outputPath = sourcePath;
#ifdef _WIN32
        outputPath.replace_extension(".exe");
#else
        outputPath.replace_extension("");
#endif

        std::optional<std::filesystem::path> requestedCompiler;
        std::optional<std::filesystem::path> emittedCpp;
        bool keepCpp = false;
        bool quiet = false;

        for (; index < argc; ++index) {
            const std::string option = argv[index];
            if (option == "-o" && index + 1 < argc) {
                outputPath = argv[++index];
            } else if (option == "--cxx" && index + 1 < argc) {
                requestedCompiler = argv[++index];
            } else if (option == "--quiet") {
                quiet = true;
            } else if (option == "--emit-cpp") {
                keepCpp = true;
                if (index + 1 < argc && std::string(argv[index + 1]).rfind("-", 0) != 0) {
                    emittedCpp = argv[++index];
                }
            } else if (option.rfind("--emit-cpp=", 0) == 0) {
                keepCpp = true;
                emittedCpp = option.substr(11);
            } else {
                std::cerr << "error: unknown or incomplete option: " << option << '\n';
                return CliError;
            }
        }

        zlang::Diagnostics diagnostics;
        zlang::Program program;
        std::unordered_set<std::string> loading;
        std::unordered_set<std::string> loaded;
        std::unordered_map<std::string, std::string> modulePaths;

        if (!quiet) std::cout << Version << "\nCompiling " << sourcePath.string() << "...\n[1/5] Lexing\n";
        loadSourceTree(sourcePath,
                       {},
                       true,
                       {},
                       diagnostics,
                       program,
                       loading,
                       loaded,
                       modulePaths);
        if (diagnostics.hasErrors()) {
            diagnostics.print();
            return SourceError;
        }

        if (!quiet) std::cout << "[3/5] Semantic analysis\n";
        zlang::SemanticAnalyzer analyzer(diagnostics);
        analyzer.analyze(program);
        if (diagnostics.hasErrors()) {
            diagnostics.print();
            return SourceError;
        }

        const auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const std::filesystem::path cppPath = emittedCpp.value_or(
            std::filesystem::temp_directory_path() / ("zlang_" + std::to_string(unique) + ".cpp"));
        std::filesystem::path temporaryOutput = outputPath;
        temporaryOutput += ".tmp.exe";

        if (!quiet) std::cout << "[4/5] C++ code generation\n";
        zlang::CppCodeGenerator generator;
        if (!generator.generate(program, cppPath)) {
            std::cerr << "error: cannot write generated C++: " << cppPath.string() << '\n';
            return InternalError;
        }

        std::vector<std::string> triedCompilers;
        const auto compiler = zlang::findCompiler(requestedCompiler, triedCompilers);
        if (compiler.empty()) {
            std::cerr << "error: C++ toolchain not found. Tried:";
            for (const auto& tried : triedCompilers) std::cerr << " " << tried;
            std::cerr << "\nInstall Visual Studio 'Desktop development with C++' or use --cxx <path>.\n";
            return ToolchainError;
        }

        if (!quiet) {
            std::cout << "[5/5] Native compilation\n";
            std::cout << "Toolchain: " << compiler.string() << "\n";
        }
        std::vector<std::string> arguments;
        if (isMsvc(compiler)) {
            arguments = {"/nologo", "/std:c++17", "/utf-8", "/EHsc", "/O2",
                         cppPath.string(), "/Fe:" + temporaryOutput.string()};
        } else {
            arguments = {"-std=c++17", "-O2", cppPath.string(), "-o", temporaryOutput.string()};
        }

        std::filesystem::remove(temporaryOutput);
        const auto result = zlang::runProcess(compiler, arguments);
        if (result.exitCode != 0 || !std::filesystem::exists(temporaryOutput)) {
            std::filesystem::remove(temporaryOutput);
            std::cerr << "error: native compiler failed\nCompiler: " << compiler.string()
                      << "\nExit code: " << result.exitCode << '\n';
            if (!result.errorMessage.empty()) std::cerr << result.errorMessage << '\n';
            return ToolchainError;
        }

        std::error_code error;
        std::filesystem::remove(outputPath, error);
        error.clear();
        std::filesystem::rename(temporaryOutput, outputPath, error);
        if (error) {
            std::cerr << "error: cannot replace output file: " << error.message() << '\n';
            return ToolchainError;
        }

        if (!keepCpp && !emittedCpp) std::filesystem::remove(cppPath);
        if (!quiet) std::cout << "Build succeeded.\nOutput: " << outputPath.string() << '\n';

        if (runAfterBuild) {
            return zlang::runProcess(std::filesystem::absolute(outputPath), {}).exitCode;
        }
        return Success;
    } catch (const std::exception& error) {
        std::cerr << "internal compiler error: " << error.what() << '\n';
        return InternalError;
    }
}
