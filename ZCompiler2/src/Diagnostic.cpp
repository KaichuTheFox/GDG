#include "zlang/Common.h"
#include <iostream>
#include <sstream>
namespace zlang {void Diagnostics::error(SourceSpan s,std::string m,std::string h){items_.push_back({Severity::Error,std::move(s),std::move(m),std::move(h)});}bool Diagnostics::hasErrors()const{return !items_.empty();}void Diagnostics::print()const{for(auto&x:items_){auto l=x.span.begin.line,c=x.span.begin.column;std::cerr<<x.span.begin.file<<":"<<l<<":"<<c<<": error: "<<x.message<<"\n";std::istringstream in(source_);std::string row;for(std::uint32_t n=1;n<=l&&std::getline(in,row);++n)if(n==l){std::cerr<<row<<"\n"<<std::string(c?c-1:0,' ')<<"^\n";}if(!x.hint.empty())std::cerr<<"hint: "<<x.hint<<"\n";}}}
