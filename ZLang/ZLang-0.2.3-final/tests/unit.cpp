#include "zlang/Lexer.h"
#include "zlang/Parser.h"

#include <iostream>
#include <string>

int main()
{
    const std::string source = "main() {\nprint(\"ok\\n\")\n}\n";
    zlang::Diagnostics diagnostics(source);
    zlang::Lexer lexer("unit.z", source, diagnostics);
    zlang::Parser parser(lexer.scan(), diagnostics);
    const auto program = parser.parse();
    if (diagnostics.hasErrors() || program.functions.size() != 1) {
        diagnostics.print();
        return 1;
    }
    std::cout << "unit ok\n";
    return 0;
}
