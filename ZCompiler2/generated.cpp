#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#ifdef _WIN32
#include <windows.h>
#endif
using I=std::int64_t;using D=double;using S=std::string;using C=std::complex<double>;using V=std::variant<std::monostate,I,D,S,bool,C>;
static double num(const V&v){if(auto x=std::get_if<I>(&v))return(double)*x;if(auto x=std::get_if<D>(&v))return*x;throw std::runtime_error("numeric value required");}static bool truth(const V&v){if(auto x=std::get_if<bool>(&v))return*x;throw std::runtime_error("bool required");}static V make_cpx(const V&a,const V&b){return C(num(a),num(b));}static I range_end(const V&v){double d=num(v);if(!std::isfinite(d)||std::floor(d)!=d)throw std::runtime_error("range end must be an integer");return d<=0?0:(I)d;}static V un(const char*o,const V&a){double x=num(a);if(std::string(o)=="+")return a;return std::holds_alternative<I>(a)?V(-std::get<I>(a)):V(-x);}static V bin(const std::string&o,const V&a,const V&b){if(o=="==")return a==b;if(o=="!=")return a!=b;if((o=="+"||o=="-"||o=="*"||o=="/")&&std::holds_alternative<C>(a)&&std::holds_alternative<C>(b)){auto x=std::get<C>(a),y=std::get<C>(b);if(o=="+")return x+y;if(o=="-")return x-y;if(o=="*")return x*y;if(y==C{})throw std::runtime_error("division by zero");return x/y;}if(o=="+"&&std::holds_alternative<S>(a)&&std::holds_alternative<S>(b))return std::get<S>(a)+std::get<S>(b);double x=num(a),y=num(b);if(o=="<")return x<y;if(o=="<=")return x<=y;if(o==">")return x>y;if(o==">=")return x>=y;if(o=="/"&&y==0)throw std::runtime_error("division by zero");if(o=="/")return x/y;bool ints=std::holds_alternative<I>(a)&&std::holds_alternative<I>(b);if(o=="+")return ints?V(std::get<I>(a)+std::get<I>(b)):V(x+y);if(o=="-")return ints?V(std::get<I>(a)-std::get<I>(b)):V(x-y);if(o=="*")return ints?V(std::get<I>(a)*std::get<I>(b)):V(x*y);throw std::runtime_error("invalid operator");}static std::string text(const V&v){if(auto x=std::get_if<I>(&v))return std::to_string(*x);if(auto x=std::get_if<D>(&v)){std::ostringstream o;o<<std::setprecision(15)<<*x;return o.str();}if(auto x=std::get_if<S>(&v))return*x;if(auto x=std::get_if<bool>(&v))return*x?"true":"false";if(auto x=std::get_if<C>(&v)){std::ostringstream o;o<<x->real()<<(x->imag()<0?"":"+")<<x->imag()<<"i";return o.str();}return"";}static void zprint(const V&v){std::cout<<text(v);}
static V z_main();
static V z_main(){
  zprint(V(S{"hello world"}));
return V{};
}
int main(){
#ifdef _WIN32
SetConsoleOutputCP(CP_UTF8);SetConsoleCP(CP_UTF8);
#endif
try{z_main();return 0;}catch(const std::exception&e){std::cerr<<"runtime error: "<<e.what()<<"\n";return 5;}}
