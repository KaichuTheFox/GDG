#pragma once

#include "zlang/AST.h"

#include <optional>
#include <unordered_map>

namespace zlang {

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(Diagnostics& diagnostics);
    bool analyze(Program& program);

private:
    struct VariableSymbol {
        Type type = Type::Unknown;
        bool isReadOnly = false;
        TypeSpec spec;
    };

    void analyzeStatement(Stmt& statement, Type& returnType);
    Type analyzeExpression(Expr& expression, bool valueRequired = true);
    [[nodiscard]] bool typesCompatible(Type expected, Type actual) const;
    [[nodiscard]] std::string functionKey(const Function& function) const;
    void pushScope();
    void popScope();
    [[nodiscard]] std::optional<VariableSymbol> findVariable(const std::string& name) const;
    [[nodiscard]] std::optional<TypeSpec> typeSpecOf(const Expr& expression) const;

    Diagnostics& diagnostics_;
    std::unordered_map<std::string, Function*> functions_;
    std::vector<std::unordered_map<std::string, VariableSymbol>> scopes_;
    Function* currentFunction_ = nullptr;
    std::size_t loopDepth_ = 0;
};

} // namespace zlang
