#pragma once

#include "zlang/Common.h"

namespace zlang {

class Lexer {
public:
    Lexer(std::string fileName, std::string source, Diagnostics& diagnostics);
    std::vector<Token> scan();

private:
    [[nodiscard]] char peek(std::size_t lookahead = 0) const;
    char take();
    void addToken(TokenKind kind, std::size_t startOffset, const SourceLocation& begin);
    void scanString(const SourceLocation& begin);

    std::string fileName_;
    std::string source_;
    Diagnostics& diagnostics_;
    std::vector<Token> tokens_;
    std::size_t offset_ = 0;
    std::uint32_t line_ = 1;
    std::uint32_t column_ = 1;
    int parenthesisDepth_ = 0;
};

} // namespace zlang
