#include "zlang/Common.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace zlang {

Diagnostics::Diagnostics(std::string source) : source_(std::move(source))
{
    std::istringstream input(source_);
    std::string line;
    while (std::getline(input, line)) {
        lines_.push_back(line);
    }
    if (lines_.empty()) {
        lines_.emplace_back();
    }
}

void Diagnostics::addSource(const std::string& file, const std::string& source)
{
    std::istringstream input(source);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(input, line)) lines.push_back(line);
    if (lines.empty()) lines.emplace_back();
    sourceLines_[file] = std::move(lines);
}

void Diagnostics::error(SourceSpan span, std::string message, std::string hint)
{
    items_.push_back({Severity::Error, std::move(span), std::move(message), std::move(hint)});
}

bool Diagnostics::hasErrors() const
{
    return std::any_of(items_.begin(), items_.end(), [](const Diagnostic& item) {
        return item.severity == Severity::Error;
    });
}

const std::vector<Diagnostic>& Diagnostics::all() const
{
    return items_;
}

void Diagnostics::print() const
{
    for (const auto& item : items_) {
        const auto& start = item.span.begin;
        if (!start.file.empty()) {
            std::cerr << start.file;
            if (start.line > 0) {
                std::cerr << ':' << start.line << ':' << start.column;
            }
            std::cerr << ": ";
        }

        std::cerr << "error: " << item.message << '\n';

        const std::vector<std::string>* sourceLines = &lines_;
        const auto fileLines = sourceLines_.find(start.file);
        if (fileLines != sourceLines_.end()) sourceLines = &fileLines->second;
        if (start.line > 0 && start.line <= sourceLines->size()) {
            const std::string& sourceLine = (*sourceLines)[start.line - 1];
            std::cerr << sourceLine << '\n';
            const std::size_t column = start.column > 0 ? start.column - 1 : 0;
            std::size_t width = 1;
            if (item.span.end.line == start.line && item.span.end.column > start.column) {
                width = item.span.end.column - start.column;
            }
            std::cerr << std::string(column, ' ') << '^' << std::string(width - 1, '~') << '\n';
        }

        if (!item.hint.empty()) {
            std::cerr << "hint: " << item.hint << '\n';
        }
    }
}

} // namespace zlang
