#include "zlang/Lexer.h"
#include "zlang/Parser.h"
#include <iostream>
int main(){std::string s="main(){\nprint(\"ok\\n\")\n}\n";zlang::Diagnostics d(s);zlang::Lexer l("unit.z",s,d);auto t=l.scan();zlang::Parser p(std::move(t),d);auto a=p.parse();if(d.hasErrors()||a.functions.size()!=1)return 1;std::cout<<"unit ok\n";return 0;}
