#include "zlang/Lexer.h"

#include <cctype>
#include <unordered_map>

namespace zlang {
namespace {

const std::unordered_map<std::string, TokenKind> keywords = {
    {"box", TokenKind::Box},       {"float", TokenKind::Float},
    {"conda", TokenKind::Conda},   {"bool", TokenKind::Bool},
    {"complex", TokenKind::Complex}, {"vector", TokenKind::Vector},
    {"array", TokenKind::Array},
    {"void", TokenKind::Void},
    {"main", TokenKind::Main},     {"if", TokenKind::If},
    {"else", TokenKind::Else},     {"for", TokenKind::For},
    {"in", TokenKind::In},         {"while", TokenKind::While},
    {"return", TokenKind::Return}, {"print", TokenKind::Print},
    {"brk", TokenKind::Brk},       {"continue", TokenKind::Continue},
    {"import", TokenKind::Import},
    {"input", TokenKind::Input},
    {"type", TokenKind::Type},     {"or", TokenKind::Or},
    {"true", TokenKind::True},     {"false", TokenKind::False},
};

bool isAsciiLetter(char value)
{
    const auto c = static_cast<unsigned char>(value);
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

} // namespace

Lexer::Lexer(std::string fileName, std::string source, Diagnostics& diagnostics)
    : fileName_(std::move(fileName)), source_(std::move(source)), diagnostics_(diagnostics)
{
    if (source_.rfind("\xEF\xBB\xBF", 0) == 0) {
        source_.erase(0, 3);
    }
}

char Lexer::peek(std::size_t lookahead) const
{
    const std::size_t index = offset_ + lookahead;
    return index < source_.size() ? source_[index] : '\0';
}

char Lexer::take()
{
    const char value = peek();
    if (value == '\0') {
        return value;
    }

    ++offset_;
    if (value == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return value;
}

void Lexer::addToken(TokenKind kind, std::size_t startOffset, const SourceLocation& begin)
{
    tokens_.push_back({kind,
                       source_.substr(startOffset, offset_ - startOffset),
                       {begin, {fileName_, offset_, line_, column_}}});
}

void Lexer::scanString(const SourceLocation& begin)
{
    take();
    std::string value;
    bool valid = true;

    while (peek() != '\0' && peek() != '"' && peek() != '\n') {
        const char character = take();
        if (character != '\\') {
            value.push_back(character);
            continue;
        }

        const char escape = take();
        switch (escape) {
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case '\\': value.push_back('\\'); break;
        case '"': value.push_back('"'); break;
        case '0': value.push_back('\0'); break;
        default:
            diagnostics_.error({begin, {fileName_, offset_, line_, column_}},
                               "unknown escape sequence");
            valid = false;
            break;
        }
    }

    if (peek() != '"') {
        diagnostics_.error({begin, {fileName_, offset_, line_, column_}}, "unterminated string");
        return;
    }

    take();
    if (valid) {
        tokens_.push_back({TokenKind::String, value, {begin, {fileName_, offset_, line_, column_}}});
    }
}

std::vector<Token> Lexer::scan()
{
    while (peek() != '\0') {
        const std::size_t startOffset = offset_;
        const SourceLocation begin{fileName_, offset_, line_, column_};
        const char current = peek();

        if (current == ' ' || current == '\t' || current == '\r') {
            take();
            continue;
        }

        if (current == '/' && peek(1) == '/') {
            while (peek() != '\0' && peek() != '\n') {
                take();
            }
            continue;
        }

        if (current == '\n') {
            take();
            if (parenthesisDepth_ == 0) {
                addToken(TokenKind::NewLine, startOffset, begin);
            }
            continue;
        }

        if (current == '"') {
            scanString(begin);
            continue;
        }

        if (isAsciiLetter(current) || current == '_') {
            take();
            while (isAsciiLetter(peek()) || std::isdigit(static_cast<unsigned char>(peek())) ||
                   peek() == '_') {
                take();
            }
            const std::string text = source_.substr(startOffset, offset_ - startOffset);
            const auto keyword = keywords.find(text);
            addToken(keyword == keywords.end() ? TokenKind::Identifier : keyword->second,
                     startOffset,
                     begin);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(current))) {
            take();
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                take();
            }

            bool floating = false;
            if (peek() == '.') {
                floating = true;
                take();
                if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                    diagnostics_.error({begin, {fileName_, offset_, line_, column_}},
                                       "invalid number; expected digit after decimal point");
                }
                while (std::isdigit(static_cast<unsigned char>(peek()))) {
                    take();
                }
            }

            if (isAsciiLetter(peek()) || peek() == '_') {
                while (isAsciiLetter(peek()) || std::isdigit(static_cast<unsigned char>(peek())) ||
                       peek() == '_') {
                    take();
                }
                diagnostics_.error({begin, {fileName_, offset_, line_, column_}},
                                   "invalid numeric literal");
            }

            addToken(floating ? TokenKind::Floating : TokenKind::Integer, startOffset, begin);
            continue;
        }

        auto single = [&](TokenKind kind) {
            take();
            addToken(kind, startOffset, begin);
        };

        switch (current) {
        case '+': single(TokenKind::Plus); break;
        case '-': single(TokenKind::Minus); break;
        case '*': single(TokenKind::Star); break;
        case '/': single(TokenKind::Slash); break;
        case '%': single(TokenKind::Percent); break;
        case '&':
            take();
            if (peek() == '&') {
                take();
                addToken(TokenKind::AndAnd, startOffset, begin);
            } else {
                diagnostics_.error({begin, {fileName_, offset_, line_, column_}},
                                   "expected '&&'");
            }
            break;
        case '(':
            ++parenthesisDepth_;
            single(TokenKind::LParen);
            break;
        case ')':
            if (parenthesisDepth_ == 0) {
                diagnostics_.error({begin, begin}, "unmatched ')'");
            } else {
                --parenthesisDepth_;
            }
            single(TokenKind::RParen);
            break;
        case '{': single(TokenKind::LBrace); break;
        case '}': single(TokenKind::RBrace); break;
        case '[': single(TokenKind::LBracket); break;
        case ']': single(TokenKind::RBracket); break;
        case '.': single(TokenKind::Dot); break;
        case ',': single(TokenKind::Comma); break;
        case ';': single(TokenKind::Semicolon); break;
        case '=':
            take();
            if (peek() == '=') {
                take();
                addToken(TokenKind::EqEq, startOffset, begin);
            } else {
                addToken(TokenKind::Assign, startOffset, begin);
            }
            break;
        case '!':
            take();
            if (peek() == '=') {
                take();
                addToken(TokenKind::NotEq, startOffset, begin);
            } else {
                addToken(TokenKind::Bang, startOffset, begin);
            }
            break;
        case '<':
            take();
            if (peek() == '=') {
                take();
                addToken(TokenKind::LessEq, startOffset, begin);
            } else {
                addToken(TokenKind::Less, startOffset, begin);
            }
            break;
        case '>':
            take();
            if (peek() == '=') {
                take();
                addToken(TokenKind::GreaterEq, startOffset, begin);
            } else {
                addToken(TokenKind::Greater, startOffset, begin);
            }
            break;
        default:
            take();
            diagnostics_.error({begin, {fileName_, offset_, line_, column_}}, "invalid character");
            break;
        }
    }

    const SourceLocation end{fileName_, offset_, line_, column_};
    tokens_.push_back({TokenKind::Eof, {}, {end, end}});
    return tokens_;
}

} // namespace zlang
