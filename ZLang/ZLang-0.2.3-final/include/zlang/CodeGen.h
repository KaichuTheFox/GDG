#pragma once

#include "zlang/AST.h"

#include <filesystem>
#include <string>

namespace zlang {

class CodeGenerator {
public:
    virtual ~CodeGenerator() = default;
    virtual bool generate(const Program& program, const std::filesystem::path& outputPath) = 0;
};

class CppCodeGenerator final : public CodeGenerator {
public:
    bool generate(const Program& program, const std::filesystem::path& outputPath) override;

private:
    std::string generateExpression(const Expr& expression);
    std::string generateTypedInitializer(const Expr& expression, const TypeSpec& type);
    void generateStatement(const Stmt& statement, int indentation);
    static std::string escapeString(const std::string& value);
    std::string output_;
};

} // namespace zlang
