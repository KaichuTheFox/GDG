#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace zlang {

struct SourceLocation {
    std::string file;
    std::size_t offset = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

struct SourceSpan {
    SourceLocation begin;
    SourceLocation end;
};

enum class Severity { Error, Warning, Note };

struct Diagnostic {
    Severity severity = Severity::Error;
    SourceSpan span;
    std::string message;
    std::string hint;
};

class Diagnostics {
public:
    explicit Diagnostics(std::string source = {});

    void addSource(const std::string& file, const std::string& source);
    void error(SourceSpan span, std::string message, std::string hint = {});
    [[nodiscard]] bool hasErrors() const;
    void print() const;
    [[nodiscard]] const std::vector<Diagnostic>& all() const;

private:
    std::string source_;
    std::vector<std::string> lines_;
    std::unordered_map<std::string, std::vector<std::string>> sourceLines_;
    std::vector<Diagnostic> items_;
};

enum class TokenKind {
    Identifier, Integer, Floating, String, True, False,
    Box, Float, Conda, Bool, Complex, Vector, Array, Void, Main, If, Else, For, In, While, Return, Print,
    Brk, Continue, Import, Input, Type, Or,
    Plus, Minus, Star, Slash, Percent, AndAnd, Bang, EqEq, NotEq, Less, Greater, LessEq, GreaterEq, Assign,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, Dot,
    Comma, Semicolon, NewLine, Eof
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string lexeme;
    SourceSpan span;
};

} // namespace zlang
