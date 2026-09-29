#pragma once
#include "zlang/Common.h"
namespace zlang { class Lexer { public: Lexer(std::string file,std::string source,Diagnostics& d); std::vector<Token> scan(); private: char peek(std::size_t n=0)const; char take(); void token(TokenKind,std::size_t,SourceLocation); void stringToken(std::size_t,SourceLocation); std::string file_,src_;Diagnostics& d_;std::vector<Token> out_;std::size_t i_{};std::uint32_t line_{1},col_{1};int parens_{};}; }
