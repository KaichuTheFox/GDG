#pragma once

#include "zlang/AST.h"

namespace zlang {

class Parser {
public:
    Parser(std::vector<Token> tokens, Diagnostics& diagnostics);
    Program parse();

private:
    [[nodiscard]] const Token& peek(std::size_t lookahead = 0) const;
    [[nodiscard]] bool at(TokenKind kind) const;
    bool match(TokenKind kind);
    const Token& expect(TokenKind kind, const char* message);
    void consumeTerminators();
    void synchronizeStatement();

    std::unique_ptr<Function> parseFunction();
    ImportDecl parseImport();
    std::unique_ptr<Block> parseBlock();
    StmtPtr parseStatement();
    TypeSpec parseTypeSpec();
    bool startsType(TokenKind kind) const;
    ExprPtr parseExpression();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();

    std::vector<Token> tokens_;
    Diagnostics& diagnostics_;
    std::size_t current_ = 0;
    bool fatalError_ = false;
};

} // namespace zlang
