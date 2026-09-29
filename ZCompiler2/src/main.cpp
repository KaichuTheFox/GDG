#include "zlang/Lexer.h"
#include "zlang/Parser.h"
#include "zlang/Semantic.h"
#include "zlang/CodeGen.h"
#include "zlang/Process.h"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace zlang;int main(int argc,char**argv){try{if(argc<2||std::string(argv[1])=="--help"){std::cout<<"Z Language Compiler 0.2.0\nUsage: zlang [build] file.z [-o output] [--emit-cpp [file]] [--cxx path] [--quiet]\n";return argc<2?2:0;}if(std::string(argv[1])=="--version"){std::cout<<"Z Language Compiler 0.2.0\n";return 0;}int i=1;if(std::string(argv[i])=="build")++i;if(i>=argc){std::cerr<<"missing source file\n";return 2;}std::filesystem::path src=argv[i++],out=src;out.replace_extension(
#ifdef _WIN32
".exe"
#else
""
#endif
);std::optional<std::filesystem::path>cxx,emit;bool keep=false,quiet=false;for(;i<argc;++i){std::string a=argv[i];if(a=="-o"&&i+1<argc)out=argv[++i];else if(a=="--cxx"&&i+1<argc)cxx=argv[++i];else if(a=="--quiet")quiet=true;else if(a=="--emit-cpp"){keep=true;if(i+1<argc&&std::string(argv[i+1]).rfind("-",0)!=0)emit=argv[++i];}else{std::cerr<<"unknown option: "<<a<<"\n";return 2;}}std::ifstream f(src,std::ios::binary);if(!f){std::cerr<<"cannot open source: "<<src<<"\n";return 1;}std::ostringstream ss;ss<<f.rdbuf();auto text=ss.str();Diagnostics d(text);if(!quiet)std::cout<<"Z Language Compiler 0.2.0\nCompiling "<<src.string()<<"...\n[1/5] Lexing\n";Lexer l(src.string(),text,d);auto tok=l.scan();if(!quiet)std::cout<<"[2/5] Parsing\n";Parser p(std::move(tok),d);auto prog=p.parse();if(!quiet)std::cout<<"[3/5] Semantic analysis\n";SemanticAnalyzer sem(d);sem.analyze(prog);if(d.hasErrors()){d.print();return 1;}auto cpp=emit.value_or(std::filesystem::temp_directory_path()/"zlang_generated.cpp");if(!quiet)std::cout<<"[4/5] C++ code generation\n";CppCodeGenerator gen;if(!gen.generate(prog,cpp)){std::cerr<<"cannot write generated C++\n";return 4;}std::vector<std::string>tried;auto compiler=findCompiler(cxx,tried);if(compiler.empty()){std::cerr<<"C++ toolchain not found. Tried:";for(auto&s:tried)std::cerr<<" "<<s;std::cerr<<"\nUse --cxx <path>. Install MSVC Build Tools, Clang, or MinGW.\n";return 3;}if(!quiet)std::cout<<"[5/5] Native compilation\n";std::vector<std::string>a;
#ifdef _WIN32
a=compiler.filename()=="cl.exe"?std::vector<std::string>{"/nologo","/std:c++17","/utf-8","/EHsc",cpp.string(),("/Fe:"+out.string())}:std::vector<std::string>{"-std=c++17","-O2",cpp.string(),"-o",out.string()};
#else
a={"-std=c++17","-O2",cpp.string(),"-o",out.string()};
#endif
auto r=runProcess(compiler,a);if(r.exitCode){std::cerr<<"native compiler failed\nCommand: "<<r.command<<"\nExit code: "<<r.exitCode<<"\n";return 3;}if(!keep&&!emit)std::filesystem::remove(cpp);if(!quiet)std::cout<<"Build succeeded.\nOutput: "<<out.string()<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"internal compiler error: "<<e.what()<<"\n";return 4;}}
