#pragma once

#include "zlang/Common.h"

#include <complex>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zlang {

enum class Type { Unknown, Box, Float, Conda, Bool, Complex, Vector, Array, Void };

struct TypeSpec {
    Type kind = Type::Unknown;
    std::shared_ptr<TypeSpec> element;
    std::size_t arraySize = 0;
};

struct Expr {
    SourceSpan span;
    Type inferred = Type::Unknown;
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct LiteralExpr final : Expr { std::variant<std::int64_t, double, std::string, bool> value; };
struct NameExpr final : Expr { std::string name; };
struct UnaryExpr final : Expr { std::string op; ExprPtr rhs; };
struct BinaryExpr final : Expr { ExprPtr lhs; std::string op; ExprPtr rhs; };
struct CallExpr final : Expr {
    std::string callee;
    std::string resolvedCallee;
    std::vector<ExprPtr> args;
};
struct VectorLiteralExpr final : Expr { std::vector<ExprPtr> elements; };
struct IndexExpr final : Expr { ExprPtr collection; ExprPtr index; };
struct MemberCallExpr final : Expr {
    ExprPtr object;
    std::string member;
    std::vector<ExprPtr> args;
};

struct Stmt {
    SourceSpan span;
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct Block final : Stmt { std::vector<StmtPtr> statements; };
struct VarDecl final : Stmt { TypeSpec type; std::string name; ExprPtr init; };
struct Assign final : Stmt { std::string name; ExprPtr value; };
struct IndexAssign final : Stmt { ExprPtr target; ExprPtr value; };
struct Print final : Stmt { std::vector<ExprPtr> args; };
struct If final : Stmt {
    ExprPtr cond;
    std::unique_ptr<Block> thenBlock;
    std::vector<std::pair<ExprPtr, std::unique_ptr<Block>>> elseIfs;
    std::unique_ptr<Block> elseBlock;
};
struct While final : Stmt { ExprPtr cond; std::unique_ptr<Block> body; };
struct For final : Stmt {
    std::string name;
    ExprPtr start;
    ExprPtr end;
    ExprPtr step;
    std::unique_ptr<Block> body;
};
struct Break final : Stmt {};
struct Continue final : Stmt {};
struct Return final : Stmt { ExprPtr value; };
struct ExprStmt final : Stmt { ExprPtr expr; };

struct Function {
    SourceSpan span;
    std::string name;
    std::vector<std::string> params;
    std::vector<TypeSpec> paramTypes;
    std::unique_ptr<Block> body;
    Type returnType = Type::Unknown;
    TypeSpec returnSpec;
    bool hasReturnType = false;
    std::string module;
};

struct ImportDecl { std::string module; SourceSpan span; };
struct Program {
    std::vector<ImportDecl> imports;
    std::vector<std::unique_ptr<Function>> functions;
};

} // namespace zlang
