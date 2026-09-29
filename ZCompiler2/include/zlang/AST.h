#pragma once
#include "zlang/Common.h"
namespace zlang {
enum class Type { Unknown,Box,Line,Bool,Cpx,Void };
struct Expr { SourceSpan span; Type inferred{Type::Unknown}; virtual ~Expr()=default; };
using ExprPtr=std::unique_ptr<Expr>;
struct LiteralExpr:Expr { std::variant<std::int64_t,double,std::string,bool> value; };
struct NameExpr:Expr { std::string name; };
struct UnaryExpr:Expr { std::string op;ExprPtr rhs; };
struct BinaryExpr:Expr { ExprPtr lhs;std::string op;ExprPtr rhs; };
struct CallExpr:Expr { std::string callee;std::vector<ExprPtr> args; };
struct Stmt { SourceSpan span; virtual ~Stmt()=default; }; using StmtPtr=std::unique_ptr<Stmt>;
struct Block:Stmt { std::vector<StmtPtr> statements; };
struct VarDecl:Stmt { Type type;std::string name;ExprPtr init; };
struct Assign:Stmt { std::string name;ExprPtr value; };
struct Print:Stmt { std::vector<ExprPtr> args; };
struct If:Stmt { ExprPtr cond;std::unique_ptr<Block> thenBlock;std::vector<std::pair<ExprPtr,std::unique_ptr<Block>>> elseIfs;std::unique_ptr<Block> elseBlock; };
struct While:Stmt { ExprPtr cond;std::unique_ptr<Block> body; };
struct For:Stmt { std::string name;ExprPtr end;std::unique_ptr<Block> body; };
struct Return:Stmt { ExprPtr value; };
struct ExprStmt:Stmt { ExprPtr expr; };
struct Function { SourceSpan span;std::string name;std::vector<std::string> params;std::unique_ptr<Block> body;Type returnType{Type::Unknown}; };
struct Program { std::vector<std::unique_ptr<Function>> functions; };
}
