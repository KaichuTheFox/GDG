#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
namespace zlang {
struct SourceLocation { std::string file; std::size_t offset{}; std::uint32_t line{1}, column{1}; };
struct SourceSpan { SourceLocation begin, end; };
enum class Severity { Error, Warning, Note };
struct Diagnostic { Severity severity{Severity::Error}; SourceSpan span; std::string message, hint; };
class Diagnostics { public: explicit Diagnostics(std::string source={}):source_(std::move(source)){} void error(SourceSpan,std::string,std::string={}); bool hasErrors() const; void print() const; const auto& all()const{return items_;} private:std::string source_;std::vector<Diagnostic> items_;};
enum class TokenKind { Identifier,Integer,Floating,String,True,False,Box,Line,Bool,Cpx,Main,If,Else,For,In,While,Return,Print,Plus,Minus,Star,Slash,EqEq,NotEq,Less,Greater,LessEq,GreaterEq,Assign,LParen,RParen,LBrace,RBrace,Comma,Semicolon,NewLine,Eof };
struct Token { TokenKind kind; std::string lexeme; SourceSpan span; };
}
