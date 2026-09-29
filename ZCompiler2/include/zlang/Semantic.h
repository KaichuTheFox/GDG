#pragma once
#include "zlang/AST.h"
namespace zlang { class SemanticAnalyzer { public:explicit SemanticAnalyzer(Diagnostics&d):d_(d){} bool analyze(Program&);private:struct Var{Type type;bool readonly;};void stmt(Stmt&,Type&);Type expr(Expr&,bool value=true);bool compatible(Type,Type)const;void push();void pop();std::optional<Var> find(const std::string&)const;Diagnostics&d_;std::unordered_map<std::string,Function*> funcs_;std::vector<std::unordered_map<std::string,Var>> scopes_;Function* current_{};}; }
