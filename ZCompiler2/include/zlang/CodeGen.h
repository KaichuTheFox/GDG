#pragma once
#include "zlang/AST.h"
namespace zlang { class CodeGenerator{public:virtual~CodeGenerator()=default;virtual bool generate(const Program&,const std::filesystem::path&)=0;};class CppCodeGenerator final:public CodeGenerator{public:bool generate(const Program&,const std::filesystem::path&)override;private:std::string expr(const Expr&);void stmt(const Stmt&,int);std::string esc(const std::string&);std::string out_;}; }
