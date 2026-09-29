#pragma once
#include "zlang/AST.h"
namespace zlang { class Parser { public:Parser(std::vector<Token> t,Diagnostics&d):t_(std::move(t)),d_(d){} Program parse();private:const Token& p(std::size_t=0)const;bool at(TokenKind)const;bool take(TokenKind);const Token& need(TokenKind,const char*);void terms();std::unique_ptr<Function> function();std::unique_ptr<Block> block();StmtPtr statement();ExprPtr expression();ExprPtr equality();ExprPtr comparison();ExprPtr additive();ExprPtr multiply();ExprPtr unary();ExprPtr call();ExprPtr primary();std::vector<Token>t_;Diagnostics&d_;std::size_t i_{};}; }
