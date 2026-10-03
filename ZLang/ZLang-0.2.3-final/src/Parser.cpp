#include "zlang/Parser.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace zlang {

Parser::Parser(std::vector<Token> tokens, Diagnostics& diagnostics)
    : tokens_(std::move(tokens)), diagnostics_(diagnostics)
{
}

const Token& Parser::peek(std::size_t lookahead) const
{
    return tokens_[std::min(current_ + lookahead, tokens_.size() - 1)];
}

bool Parser::at(TokenKind kind) const
{
    return peek().kind == kind;
}

bool Parser::match(TokenKind kind)
{
    if (!at(kind)) {
        return false;
    }
    ++current_;
    return true;
}

const Token& Parser::expect(TokenKind kind, const char* message)
{
    if (at(kind)) {
        return tokens_[current_++];
    }
    diagnostics_.error(peek().span, message);
    fatalError_ = true;
    return peek();
}

void Parser::consumeTerminators()
{
    while (match(TokenKind::NewLine) || match(TokenKind::Semicolon)) {
    }
}

void Parser::synchronizeStatement()
{
    while (!at(TokenKind::Eof) && !at(TokenKind::RBrace) && !at(TokenKind::NewLine) &&
           !at(TokenKind::Semicolon)) {
        ++current_;
    }
    consumeTerminators();
}

Program Parser::parse()
{
    Program program;
    consumeTerminators();

    while (!at(TokenKind::Eof)) {
        const std::size_t before = current_;
        if (at(TokenKind::Import)) {
            program.imports.push_back(parseImport());
        } else {
            auto function = parseFunction();
            if (function) {
                program.functions.push_back(std::move(function));
            }
        }
        consumeTerminators();

        if (current_ == before) {
            diagnostics_.error(peek().span, "parser could not make progress");
            ++current_;
        }
    }
    return program;
}

ImportDecl Parser::parseImport()
{
    const Token keyword = expect(TokenKind::Import, "expected 'import'");
    const Token& module = expect(TokenKind::Identifier, "module name expected after import");
    return {module.lexeme, {keyword.span.begin, module.span.end}};
}

std::unique_ptr<Function> Parser::parseFunction()
{
    auto function = std::make_unique<Function>();
    if (startsType(peek().kind)) {
        function->returnSpec = parseTypeSpec();
        function->hasReturnType = true;
        function->returnType = function->returnSpec.kind;
    }
    const Token& name = at(TokenKind::Main)
                            ? expect(TokenKind::Main, "function name expected")
                            : expect(TokenKind::Identifier, "function name expected");
    if (fatalError_ && name.kind == TokenKind::Eof) {
        return nullptr;
    }

    function->name = name.lexeme;
    function->span.begin = name.span.begin;
    expect(TokenKind::LParen, "expected '('");

    if (!at(TokenKind::RParen)) {
        const bool typedParameters = startsType(peek().kind);
        do {
            TypeSpec parameterType;
            if (typedParameters) parameterType = parseTypeSpec();
            function->paramTypes.push_back(parameterType);
            function->params.push_back(expect(TokenKind::Identifier, "parameter expected").lexeme);
            if (!typedParameters && at(TokenKind::Comma) && startsType(peek(1).kind)) {
                diagnostics_.error(peek(1).span,
                                   "all function parameters must either have types or omit them");
            }
        } while (match(TokenKind::Comma));
    }

    expect(TokenKind::RParen, "expected ')'");
    consumeTerminators();
    function->body = parseBlock();
    if (!function->body) {
        return nullptr;
    }
    function->span.end = function->body->span.end;
    fatalError_ = false;
    return function;
}

std::unique_ptr<Block> Parser::parseBlock()
{
    auto block = std::make_unique<Block>();
    const Token& opening = expect(TokenKind::LBrace, "expected '{'");
    block->span.begin = opening.span.begin;
    consumeTerminators();

    while (!at(TokenKind::RBrace) && !at(TokenKind::Eof)) {
        const std::size_t before = current_;
        block->statements.push_back(parseStatement());

        consumeTerminators();

        if (current_ == before) {
            ++current_;
        }
    }

    const Token& closing = expect(TokenKind::RBrace, "expected '}'");
    block->span.end = closing.span.end;
    return block;
}

StmtPtr Parser::parseStatement()
{
    if (at(TokenKind::Box) || at(TokenKind::Float) || at(TokenKind::Conda) ||
        at(TokenKind::Bool) || at(TokenKind::Complex) || at(TokenKind::Vector) ||
        at(TokenKind::Array)) {
        auto statement = std::make_unique<VarDecl>();
        statement->span.begin = peek().span.begin;
        statement->type = parseTypeSpec();
        statement->name = expect(TokenKind::Identifier, "variable name expected").lexeme;
        expect(TokenKind::Assign, "expected '='");
        statement->init = parseExpression();
        statement->span.end = statement->init->span.end;
        return statement;
    }

    if (match(TokenKind::Print)) {
        auto statement = std::make_unique<Print>();
        statement->span.begin = tokens_[current_ - 1].span.begin;
        const bool parenthesized = match(TokenKind::LParen);
        if (!(parenthesized && at(TokenKind::RParen)) && !at(TokenKind::NewLine) &&
            !at(TokenKind::Semicolon) && !at(TokenKind::RBrace)) {
            do {
                statement->args.push_back(parseExpression());
            } while (match(TokenKind::Comma));
        }
        if (parenthesized) {
            statement->span.end = expect(TokenKind::RParen, "expected ')'").span.end;
        } else {
            statement->span.end = peek().span.begin;
        }
        return statement;
    }

    if (match(TokenKind::If)) {
        auto statement = std::make_unique<If>();
        statement->span.begin = tokens_[current_ - 1].span.begin;
        expect(TokenKind::LParen, "expected '('");
        statement->cond = parseExpression();
        expect(TokenKind::RParen, "expected ')'");
        consumeTerminators();
        statement->thenBlock = parseBlock();
        consumeTerminators();

        while (match(TokenKind::Else)) {
            if (match(TokenKind::If)) {
                expect(TokenKind::LParen, "expected '('");
                auto condition = parseExpression();
                expect(TokenKind::RParen, "expected ')'");
                consumeTerminators();
                statement->elseIfs.push_back({std::move(condition), parseBlock()});
                consumeTerminators();
            } else {
                statement->elseBlock = parseBlock();
                break;
            }
        }
        statement->span.end = statement->elseBlock ? statement->elseBlock->span.end
                                                   : statement->thenBlock->span.end;
        return statement;
    }

    if (match(TokenKind::While)) {
        auto statement = std::make_unique<While>();
        statement->span.begin = tokens_[current_ - 1].span.begin;
        expect(TokenKind::LParen, "expected '('");
        statement->cond = parseExpression();
        expect(TokenKind::RParen, "expected ')'");
        consumeTerminators();
        statement->body = parseBlock();
        statement->span.end = statement->body->span.end;
        return statement;
    }

    if (match(TokenKind::Brk)) {
        auto statement = std::make_unique<Break>();
        statement->span = tokens_[current_ - 1].span;
        return statement;
    }

    if (match(TokenKind::Continue)) {
        auto statement = std::make_unique<Continue>();
        statement->span = tokens_[current_ - 1].span;
        return statement;
    }

    if (match(TokenKind::For)) {
        auto statement = std::make_unique<For>();
        statement->span.begin = tokens_[current_ - 1].span.begin;
        statement->name = expect(TokenKind::Identifier, "loop variable expected").lexeme;
        expect(TokenKind::In, "expected 'in'");
        auto integerLiteral = [&](std::int64_t value) {
            auto literal = std::make_unique<LiteralExpr>();
            literal->value = value;
            literal->span = statement->span;
            return ExprPtr(std::move(literal));
        };
        if (at(TokenKind::Identifier) && peek().lexeme == "range" &&
            peek(1).kind == TokenKind::LParen) {
            ++current_;
            expect(TokenKind::LParen, "expected '(' after range");
            std::vector<ExprPtr> args;
            if (!at(TokenKind::RParen)) {
                do { args.push_back(parseExpression()); } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RParen, "expected ')' after range arguments");
            if (args.empty() || args.size() > 3) {
                diagnostics_.error(statement->span,
                                   "range expects one, two, or three arguments");
            }
            if (args.size() == 1) {
                statement->start = integerLiteral(0);
                statement->end = std::move(args[0]);
                statement->step = integerLiteral(1);
            } else if (args.size() == 2) {
                statement->start = std::move(args[0]);
                statement->end = std::move(args[1]);
                statement->step = integerLiteral(1);
            } else if (args.size() >= 3) {
                statement->start = std::move(args[0]);
                statement->end = std::move(args[1]);
                statement->step = std::move(args[2]);
            } else {
                statement->start = integerLiteral(0);
                statement->end = integerLiteral(0);
                statement->step = integerLiteral(1);
            }
        } else {
            expect(TokenKind::Plus, "expected '+(' or range('");
            expect(TokenKind::LParen, "expected '('");
            statement->start = integerLiteral(0);
            statement->end = parseExpression();
            statement->step = integerLiteral(1);
            expect(TokenKind::RParen, "expected ')'");
        }
        consumeTerminators();
        statement->body = parseBlock();
        statement->span.end = statement->body->span.end;
        return statement;
    }

    if (match(TokenKind::Return)) {
        auto statement = std::make_unique<Return>();
        statement->span.begin = tokens_[current_ - 1].span.begin;
        if (!at(TokenKind::NewLine) && !at(TokenKind::Semicolon) && !at(TokenKind::RBrace)) {
            statement->value = parseExpression();
        }
        statement->span.end = statement->value ? statement->value->span.end : peek().span.begin;
        return statement;
    }

    auto target = parseExpression();
    if (match(TokenKind::Assign)) {
        auto value = parseExpression();
        if (auto* name = dynamic_cast<NameExpr*>(target.get())) {
            auto statement = std::make_unique<Assign>();
            statement->span = {target->span.begin, value->span.end};
            statement->name = name->name;
            statement->value = std::move(value);
            return statement;
        }
        if (dynamic_cast<IndexExpr*>(target.get())) {
            auto statement = std::make_unique<IndexAssign>();
            statement->span = {target->span.begin, value->span.end};
            statement->target = std::move(target);
            statement->value = std::move(value);
            return statement;
        }
        diagnostics_.error(target->span, "assignment target must be a variable or vector element");
    }
    auto statement = std::make_unique<ExprStmt>();
    statement->span = target->span;
    statement->expr = std::move(target);
    return statement;
}

TypeSpec Parser::parseTypeSpec()
{
    const Token token = peek();
    ++current_;
    TypeSpec type;
    switch (token.kind) {
    case TokenKind::Box: type.kind = Type::Box; break;
    case TokenKind::Float: type.kind = Type::Float; break;
    case TokenKind::Conda: type.kind = Type::Conda; break;
    case TokenKind::Bool: type.kind = Type::Bool; break;
    case TokenKind::Complex: type.kind = Type::Complex; break;
    case TokenKind::Vector:
        type.kind = Type::Vector;
        expect(TokenKind::Less, "expected '<' after vector");
        type.element = std::make_shared<TypeSpec>(parseTypeSpec());
        expect(TokenKind::Greater, "expected '>' after vector element type");
        break;
    case TokenKind::Array: {
        type.kind = Type::Array;
        expect(TokenKind::Less, "expected '<' after array");
        type.element = std::make_shared<TypeSpec>(parseTypeSpec());
        expect(TokenKind::Comma, "expected ',' before array size");
        const Token& size = expect(TokenKind::Integer, "array size must be an integer literal");
        try {
            const auto parsed = std::stoull(size.lexeme);
            if constexpr (sizeof(std::size_t) < sizeof(unsigned long long)) {
                if (parsed > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
                    throw std::out_of_range("array size");
                }
            }
            type.arraySize = static_cast<std::size_t>(parsed);
        } catch (...) {
            diagnostics_.error(size.span, "array size is outside supported range");
            type.arraySize = 0;
        }
        expect(TokenKind::Greater, "expected '>' after array size");
        break;
    }
    case TokenKind::Void: type.kind = Type::Void; break;
    default:
        diagnostics_.error(token.span, "expected a type name");
        type.kind = Type::Unknown;
        break;
    }
    return type;
}

bool Parser::startsType(TokenKind kind) const
{
    return kind == TokenKind::Box || kind == TokenKind::Float || kind == TokenKind::Conda ||
           kind == TokenKind::Bool || kind == TokenKind::Complex || kind == TokenKind::Vector ||
           kind == TokenKind::Array || kind == TokenKind::Void;
}

ExprPtr Parser::parseExpression() { return parseLogicalOr(); }

ExprPtr Parser::parseLogicalOr()
{
    auto expression = parseLogicalAnd();
    while (match(TokenKind::Or)) {
        const std::string op = tokens_[current_ - 1].lexeme;
        auto right = parseLogicalAnd();
        auto binary = std::make_unique<BinaryExpr>();
        binary->span = {expression->span.begin, right->span.end};
        binary->lhs = std::move(expression);
        binary->op = op;
        binary->rhs = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseLogicalAnd()
{
    auto expression = parseEquality();
    while (match(TokenKind::AndAnd)) {
        const std::string op = tokens_[current_ - 1].lexeme;
        auto right = parseEquality();
        auto binary = std::make_unique<BinaryExpr>();
        binary->span = {expression->span.begin, right->span.end};
        binary->lhs = std::move(expression);
        binary->op = op;
        binary->rhs = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseEquality()
{
    auto expression = parseComparison();
    while (at(TokenKind::EqEq) || at(TokenKind::NotEq)) {
        const std::string op = peek().lexeme;
        ++current_;
        auto right = parseComparison();
        auto binary = std::make_unique<BinaryExpr>();
        binary->span = {expression->span.begin, right->span.end};
        binary->lhs = std::move(expression);
        binary->op = op;
        binary->rhs = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseComparison()
{
    auto expression = parseAdditive();
    while (at(TokenKind::Less) || at(TokenKind::LessEq) || at(TokenKind::Greater) ||
           at(TokenKind::GreaterEq)) {
        const std::string op = peek().lexeme;
        ++current_;
        auto right = parseAdditive();
        auto binary = std::make_unique<BinaryExpr>();
        binary->span = {expression->span.begin, right->span.end};
        binary->lhs = std::move(expression);
        binary->op = op;
        binary->rhs = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseAdditive()
{
    auto expression = parseMultiplicative();
    while (at(TokenKind::Plus) || at(TokenKind::Minus)) {
        const std::string op = peek().lexeme;
        ++current_;
        auto right = parseMultiplicative();
        auto binary = std::make_unique<BinaryExpr>();
        binary->span = {expression->span.begin, right->span.end};
        binary->lhs = std::move(expression);
        binary->op = op;
        binary->rhs = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseMultiplicative()
{
    auto expression = parseUnary();
    while (at(TokenKind::Star) || at(TokenKind::Slash) || at(TokenKind::Percent)) {
        const std::string op = peek().lexeme;
        ++current_;
        auto right = parseUnary();
        auto binary = std::make_unique<BinaryExpr>();
        binary->span = {expression->span.begin, right->span.end};
        binary->lhs = std::move(expression);
        binary->op = op;
        binary->rhs = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseUnary()
{
    if (at(TokenKind::Plus) || at(TokenKind::Minus) || at(TokenKind::Bang)) {
        const Token op = peek();
        ++current_;
        auto expression = std::make_unique<UnaryExpr>();
        expression->span.begin = op.span.begin;
        expression->op = op.lexeme;
        expression->rhs = parseUnary();
        expression->span.end = expression->rhs->span.end;
        return expression;
    }
    return parseCall();
}

ExprPtr Parser::parseCall()
{
    auto expression = parsePrimary();
    while (true) {
      if (match(TokenKind::LParen)) {
        auto call = std::make_unique<CallExpr>();
        call->span.begin = expression->span.begin;
        if (const auto* name = dynamic_cast<NameExpr*>(expression.get())) {
            call->callee = name->name;
        } else {
            diagnostics_.error(expression->span, "only a function name can be called");
        }
        if (!at(TokenKind::RParen)) {
            do {
                call->args.push_back(parseExpression());
            } while (match(TokenKind::Comma));
        }
        call->span.end = expect(TokenKind::RParen, "expected ')'").span.end;
        expression = std::move(call);
      } else if (match(TokenKind::LBracket)) {
        auto index = std::make_unique<IndexExpr>();
        index->span.begin = expression->span.begin;
        index->collection = std::move(expression);
        index->index = parseExpression();
        index->span.end = expect(TokenKind::RBracket, "expected ']' after index").span.end;
        expression = std::move(index);
      } else if (match(TokenKind::Dot)) {
        const SourceLocation begin = expression->span.begin;
        auto object = std::move(expression);
        const std::string memberName = expect(TokenKind::Identifier, "member name expected").lexeme;
        expect(TokenKind::LParen, "expected '(' after member name");
        std::vector<ExprPtr> arguments;
        if (!at(TokenKind::RParen)) {
            do { arguments.push_back(parseExpression()); } while (match(TokenKind::Comma));
        }
        const Token& closing = expect(TokenKind::RParen, "expected ')' after member arguments");
        if (const auto* name = dynamic_cast<const NameExpr*>(object.get());
            name && memberName != "size" && memberName != "push" && memberName != "pop") {
            auto call = std::make_unique<CallExpr>();
            call->span = {begin, closing.span.end};
            call->callee = name->name + "." + memberName;
            call->args = std::move(arguments);
            expression = std::move(call);
        } else {
            auto member = std::make_unique<MemberCallExpr>();
            member->span = {begin, closing.span.end};
            member->object = std::move(object);
            member->member = memberName;
            member->args = std::move(arguments);
            expression = std::move(member);
        }
      } else {
        break;
      }
    }
    return expression;
}

ExprPtr Parser::parsePrimary()
{
    const Token token = peek();

    if (match(TokenKind::Integer)) {
        auto expression = std::make_unique<LiteralExpr>();
        try {
            expression->value = static_cast<std::int64_t>(std::stoll(token.lexeme));
        } catch (...) {
            diagnostics_.error(token.span, "integer literal is outside int64 range");
            expression->value = std::int64_t{0};
        }
        expression->span = token.span;
        return expression;
    }

    if (match(TokenKind::Floating)) {
        auto expression = std::make_unique<LiteralExpr>();
        try {
            const double value = std::stod(token.lexeme);
            if (!std::isfinite(value)) {
                throw std::out_of_range("floating literal");
            }
            expression->value = value;
        } catch (...) {
            diagnostics_.error(token.span, "invalid or out-of-range floating literal");
            expression->value = 0.0;
        }
        expression->span = token.span;
        return expression;
    }

    if (match(TokenKind::String)) {
        auto expression = std::make_unique<LiteralExpr>();
        expression->value = token.lexeme;
        expression->span = token.span;
        return expression;
    }

    if (match(TokenKind::LBracket)) {
        auto expression = std::make_unique<VectorLiteralExpr>();
        expression->span.begin = token.span.begin;
        if (!at(TokenKind::RBracket)) {
            do { expression->elements.push_back(parseExpression()); } while (match(TokenKind::Comma));
        }
        expression->span.end = expect(TokenKind::RBracket, "expected ']' after vector literal").span.end;
        return expression;
    }

    if (match(TokenKind::True) || match(TokenKind::False)) {
        auto expression = std::make_unique<LiteralExpr>();
        expression->value = token.kind == TokenKind::True;
        expression->span = token.span;
        return expression;
    }

    if (match(TokenKind::Identifier) || match(TokenKind::Box) || match(TokenKind::Float) ||
        match(TokenKind::Complex) || match(TokenKind::Input) || match(TokenKind::Type)) {
        auto expression = std::make_unique<NameExpr>();
        expression->name = token.lexeme;
        expression->span = token.span;
        return expression;
    }

    if (match(TokenKind::LParen)) {
        const SourceLocation begin = token.span.begin;
        auto expression = parseExpression();
        const Token& closing = expect(TokenKind::RParen, "expected ')'");
        expression->span = {begin, closing.span.end};
        return expression;
    }

    diagnostics_.error(token.span, "expected expression");
    if (!at(TokenKind::Eof)) {
        ++current_;
    }
    auto fallback = std::make_unique<LiteralExpr>();
    fallback->value = std::int64_t{0};
    fallback->span = token.span;
    return fallback;
}

} // namespace zlang
