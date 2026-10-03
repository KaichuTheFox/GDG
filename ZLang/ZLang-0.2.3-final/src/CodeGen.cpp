#include "zlang/CodeGen.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace zlang {
namespace {

std::string generatedFunctionName(const std::string& key)
{
    const auto separator = key.find('.');
    if (separator == std::string::npos) return "zfn_" + key;
    const std::string module = key.substr(0, separator);
    const std::string function = key.substr(separator + 1);
    return "zfn_m" + std::to_string(module.size()) + "_" + module + "_f" +
           std::to_string(function.size()) + "_" + function;
}

std::string generatedFunctionName(const Function& function)
{
    return generatedFunctionName(function.module.empty()
                                      ? function.name
                                      : function.module + "." + function.name);
}

} // namespace

std::string CppCodeGenerator::escapeString(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        case 0: output << "\\0"; break;
        default: output << character; break;
        }
    }
    return output.str();
}

std::string CppCodeGenerator::generateExpression(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        if (const auto* value = std::get_if<std::int64_t>(&literal->value)) {
            return "ZValue(ZInteger{" + std::to_string(*value) + "})";
        }
        if (const auto* value = std::get_if<double>(&literal->value)) {
            std::ostringstream output;
            output << std::setprecision(17) << *value;
            return "ZValue(ZFloat{" + output.str() + "})";
        }
        if (const auto* value = std::get_if<std::string>(&literal->value)) {
            return "ZValue(ZString{\"" + escapeString(*value) + "\"})";
        }
        return std::get<bool>(literal->value) ? "ZValue(true)" : "ZValue(false)";
    }
    if (const auto* name = dynamic_cast<const NameExpr*>(&expression)) {
        return "zusr_" + name->name;
    }
    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        return "zUnary(\"" + unary->op + "\", " + generateExpression(*unary->rhs) + ")";
    }
    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        if (binary->op == "&&") {
            return "ZValue(zRequireBool(" + generateExpression(*binary->lhs) +
                   ") && zRequireBool(" + generateExpression(*binary->rhs) + "))";
        }
        if (binary->op == "or") {
            return "ZValue(zRequireBool(" + generateExpression(*binary->lhs) +
                   ") || zRequireBool(" + generateExpression(*binary->rhs) + "))";
        }
        return "zBinary(\"" + binary->op + "\", " + generateExpression(*binary->lhs) +
               ", " + generateExpression(*binary->rhs) + ")";
    }
    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        if (call->callee == "input") {
            return call->args.empty() ? "zInput()" : "zInput(" + generateExpression(*call->args[0]) + ")";
        }
        if (call->callee == "type") {
            return "zTypeOf(" + generateExpression(*call->args[0]) + ")";
        }
        if (call->callee == "box") {
            return "zToBox(" + generateExpression(*call->args[0]) + ")";
        }
        if (call->callee == "float") {
            return "zToFloat(" + generateExpression(*call->args[0]) + ")";
        }
        if (call->callee == "complex") {
            const std::string real = generateExpression(*call->args[0]);
            const std::string imaginary = call->args.size() == 2
                                              ? generateExpression(*call->args[1])
                                              : "ZValue(ZFloat{0.0})";
            return "zMakeComplex(" + real + ", " + imaginary + ")";
        }

        const std::string target = call->resolvedCallee.empty() ? call->callee
                                                                : call->resolvedCallee;
        std::string output = generatedFunctionName(target) + "(";
        for (std::size_t index = 0; index < call->args.size(); ++index) {
            if (index != 0) {
                output += ", ";
            }
            output += generateExpression(*call->args[index]);
        }
        return output + ")";
    }
    if (const auto* vector = dynamic_cast<const VectorLiteralExpr*>(&expression)) {
        std::string output = "zMakeVector({";
        for (std::size_t index = 0; index < vector->elements.size(); ++index) {
            if (index != 0) output += ", ";
            output += generateExpression(*vector->elements[index]);
        }
        return output + "})";
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(&expression)) {
        return "zIndex(" + generateExpression(*index->collection) + ", " +
               generateExpression(*index->index) + ")";
    }
    if (const auto* member = dynamic_cast<const MemberCallExpr*>(&expression)) {
        if (member->member == "size") return "zSize(" + generateExpression(*member->object) + ")";
        if (member->member == "push")
            return "zPush(" + generateExpression(*member->object) + ", " +
                   generateExpression(*member->args[0]) + ")";
        if (member->member == "pop") return "zPop(" + generateExpression(*member->object) + ")";
    }
    return "ZValue{}";
}

std::string CppCodeGenerator::generateTypedInitializer(const Expr& expression, const TypeSpec& type)
{
    const auto* literal = dynamic_cast<const VectorLiteralExpr*>(&expression);
    if (!literal || !type.element || (type.kind != Type::Vector && type.kind != Type::Array)) {
        return generateExpression(expression);
    }

    std::string output = type.kind == Type::Array
                             ? "zMakeArray({"
                             : "zMakeVector({";
    for (std::size_t index = 0; index < literal->elements.size(); ++index) {
        if (index != 0) output += ", ";
        output += generateTypedInitializer(*literal->elements[index], *type.element);
    }
    output += "}";
    if (type.kind == Type::Array) output += ", " + std::to_string(type.arraySize);
    output += ")";
    return output;
}

void CppCodeGenerator::generateStatement(const Stmt& statement, int indentation)
{
    const std::string indent(static_cast<std::size_t>(indentation), ' ');
    if (const auto* declaration = dynamic_cast<const VarDecl*>(&statement)) {
        output_ += indent + "ZValue zusr_" + declaration->name + " = " +
                   generateTypedInitializer(*declaration->init, declaration->type) + ";\n";
    } else if (const auto* assignment = dynamic_cast<const Assign*>(&statement)) {
        output_ += indent + "zAssign(zusr_" + assignment->name + ", " +
                   generateExpression(*assignment->value) + ");\n";
    } else if (const auto* indexAssignment = dynamic_cast<const IndexAssign*>(&statement)) {
        const auto* index = dynamic_cast<const IndexExpr*>(indexAssignment->target.get());
        if (index) {
            output_ += indent + "zSetIndex(" + generateExpression(*index->collection) + ", " +
                       generateExpression(*index->index) + ", " +
                       generateExpression(*indexAssignment->value) + ");\n";
        }
    } else if (const auto* print = dynamic_cast<const Print*>(&statement)) {
        for (const auto& argument : print->args) {
            output_ += indent + "zPrint(" + generateExpression(*argument) + ");\n";
        }
    } else if (const auto* returnStatement = dynamic_cast<const Return*>(&statement)) {
        output_ += indent + "return " +
                   (returnStatement->value ? generateExpression(*returnStatement->value)
                                           : "ZValue{}") +
                   ";\n";
    } else if (const auto* expression = dynamic_cast<const ExprStmt*>(&statement)) {
        output_ += indent + generateExpression(*expression->expr) + ";\n";
    } else if (dynamic_cast<const Break*>(&statement)) {
        output_ += indent + "break;\n";
    } else if (dynamic_cast<const Continue*>(&statement)) {
        output_ += indent + "continue;\n";
    } else if (const auto* conditional = dynamic_cast<const If*>(&statement)) {
        output_ += indent + "if (zRequireBool(" + generateExpression(*conditional->cond) + ")) {\n";
        for (const auto& child : conditional->thenBlock->statements) {
            generateStatement(*child, indentation + 4);
        }
        output_ += indent + "}";
        for (const auto& branch : conditional->elseIfs) {
            output_ += " else if (zRequireBool(" + generateExpression(*branch.first) + ")) {\n";
            for (const auto& child : branch.second->statements) {
                generateStatement(*child, indentation + 4);
            }
            output_ += indent + "}";
        }
        if (conditional->elseBlock) {
            output_ += " else {\n";
            for (const auto& child : conditional->elseBlock->statements) {
                generateStatement(*child, indentation + 4);
            }
            output_ += indent + "}";
        }
        output_ += "\n";
    } else if (const auto* loop = dynamic_cast<const While*>(&statement)) {
        output_ += indent + "while (zRequireBool(" + generateExpression(*loop->cond) + ")) {\n";
        for (const auto& child : loop->body->statements) {
            generateStatement(*child, indentation + 4);
        }
        output_ += indent + "}\n";
    } else if (const auto* forLoop = dynamic_cast<const For*>(&statement)) {
        output_ += indent + "for (ZInteger zint_i = zRangeInteger(" +
                   generateExpression(*forLoop->start) + "), zint_n = zRangeInteger(" +
                   generateExpression(*forLoop->end) + "), zint_step = zRangeStep(" +
                   generateExpression(*forLoop->step) + "); zRangeCondition(zint_i, zint_n, zint_step); "
                   "zint_i = zRangeNext(zint_i, zint_step)) {\n";
        output_ += indent + "    ZValue zusr_" + forLoop->name + " = ZValue(zint_i);\n";
        for (const auto& child : forLoop->body->statements) {
            generateStatement(*child, indentation + 4);
        }
        output_ += indent + "}\n";
    }
}

bool CppCodeGenerator::generate(const Program& program, const std::filesystem::path& outputPath)
{
    output_ = R"CPP(#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using ZInteger = std::int64_t;
using ZFloat = double;
using ZString = std::string;
using ZComplex = std::complex<double>;
struct ZVector;
using ZVectorPtr = std::shared_ptr<ZVector>;
using ZValue = std::variant<std::monostate, ZInteger, ZFloat, ZString, bool, ZComplex, ZVectorPtr>;
struct ZVector { std::vector<ZValue> values; bool isArray = false; };

static ZValue zMakeVector(std::initializer_list<ZValue> values)
{
    return ZVectorPtr{std::make_shared<ZVector>(ZVector{std::vector<ZValue>(values), false})};
}

static ZValue zMakeArray(std::initializer_list<ZValue> values, std::size_t expectedSize)
{
    if (values.size() != expectedSize) throw std::runtime_error("array initializer size mismatch");
    return ZVectorPtr{std::make_shared<ZVector>(ZVector{std::vector<ZValue>(values), true})};
}

static ZInteger zRequireIndex(const ZValue& value)
{
    const auto* index = std::get_if<ZInteger>(&value);
    if (!index || *index < 0) throw std::runtime_error("vector index must be a nonnegative box");
    return *index;
}

static ZVectorPtr zRequireVector(const ZValue& value)
{
    const auto* vector = std::get_if<ZVectorPtr>(&value);
    if (!vector || !*vector) throw std::runtime_error("vector or array value required");
    return *vector;
}

static void zAssign(ZValue& target, const ZValue& value)
{
    const auto* current = std::get_if<ZVectorPtr>(&target);
    if (current && *current && (*current)->isArray) {
        const auto replacement = zRequireVector(value);
        if (replacement->values.size() != (*current)->values.size())
            throw std::runtime_error("array assignment size mismatch");
        for (std::size_t index = 0; index < replacement->values.size(); ++index)
            zAssign((*current)->values[index], replacement->values[index]);
        return;
    }
    target = value;
}

static ZValue zIndex(const ZValue& collection, const ZValue& indexValue)
{
    const auto vector = zRequireVector(collection);
    const auto index = zRequireIndex(indexValue);
    if (static_cast<std::uint64_t>(index) >= vector->values.size())
        throw std::runtime_error("vector index out of range");
    return vector->values[static_cast<std::size_t>(index)];
}

static void zSetIndex(const ZValue& collection, const ZValue& indexValue, const ZValue& value)
{
    const auto vector = zRequireVector(collection);
    const auto index = zRequireIndex(indexValue);
    if (static_cast<std::uint64_t>(index) >= vector->values.size())
        throw std::runtime_error("vector index out of range");
    zAssign(vector->values[static_cast<std::size_t>(index)], value);
}

static ZValue zSize(const ZValue& value)
{
    return ZInteger{static_cast<ZInteger>(zRequireVector(value)->values.size())};
}

static ZValue zPush(const ZValue& target, const ZValue& value)
{
    const auto vector = zRequireVector(target);
    if (vector->isArray) throw std::runtime_error("push cannot be used on a fixed array");
    vector->values.push_back(value);
    return ZValue{};
}

static ZValue zPop(const ZValue& target)
{
    const auto vector = zRequireVector(target);
    if (vector->isArray) throw std::runtime_error("pop cannot be used on a fixed array");
    if (vector->values.empty()) throw std::runtime_error("cannot pop from an empty vector");
    ZValue value = std::move(vector->values.back());
    vector->values.pop_back();
    return value;
}

static double zRequireNumber(const ZValue& value)
{
    if (const auto* integer = std::get_if<ZInteger>(&value)) return static_cast<double>(*integer);
    if (const auto* floating = std::get_if<ZFloat>(&value)) return *floating;
    throw std::runtime_error("numeric value required");
}

static bool zRequireBool(const ZValue& value)
{
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean;
    throw std::runtime_error("bool required");
}

static ZValue zMakeComplex(const ZValue& real, const ZValue& imaginary)
{
    return ZComplex(zRequireNumber(real), zRequireNumber(imaginary));
}

static ZInteger zRangeInteger(const ZValue& value)
{
    const auto* integer = std::get_if<ZInteger>(&value);
    if (!integer) throw std::runtime_error("range values must be box integers");
    return *integer;
}

static ZInteger zRangeStep(const ZValue& value)
{
    const ZInteger step = zRangeInteger(value);
    if (step == 0) throw std::runtime_error("range step cannot be zero");
    return step;
}

static bool zRangeCondition(ZInteger current, ZInteger end, ZInteger step)
{
    return step > 0 ? current < end : current > end;
}

static ZInteger zRangeNext(ZInteger current, ZInteger step)
{
    if ((step > 0 && current > std::numeric_limits<ZInteger>::max() - step) ||
        (step < 0 && current < std::numeric_limits<ZInteger>::min() - step)) {
        throw std::runtime_error("range iteration overflow");
    }
    return current + step;
}

static ZValue zUnary(const std::string& op, const ZValue& value)
{
    if (op == "!") return !zRequireBool(value);
    if (op == "+") return value;
    if (const auto* integer = std::get_if<ZInteger>(&value)) {
        if (*integer == std::numeric_limits<ZInteger>::min())
            throw std::runtime_error("integer overflow");
        return -*integer;
    }
    return -zRequireNumber(value);
}

static ZValue zBinary(const std::string& op, const ZValue& left, const ZValue& right)
{
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    if (op == "%") {
        const auto* x = std::get_if<ZInteger>(&left);
        const auto* y = std::get_if<ZInteger>(&right);
        if (!x || !y) throw std::runtime_error("'%' requires box integer operands");
        if (*y == 0) throw std::runtime_error("remainder by zero");
        if (*x == std::numeric_limits<ZInteger>::min() && *y == -1) return ZInteger{0};
        return ZInteger{*x % *y};
    }

    if (std::holds_alternative<ZComplex>(left) && std::holds_alternative<ZComplex>(right)) {
        const auto a = std::get<ZComplex>(left);
        const auto b = std::get<ZComplex>(right);
        if (op == "+") return a + b;
        if (op == "-") return a - b;
        if (op == "*") return a * b;
        if (op == "/") {
            if (b == ZComplex{}) throw std::runtime_error("division by zero");
            return a / b;
        }
    }

    if (op == "+" && std::holds_alternative<ZString>(left) &&
        std::holds_alternative<ZString>(right))
        return std::get<ZString>(left) + std::get<ZString>(right);

    const double a = zRequireNumber(left);
    const double b = zRequireNumber(right);
    if (op == "<") return a < b;
    if (op == "<=") return a <= b;
    if (op == ">") return a > b;
    if (op == ">=") return a >= b;
    if (op == "/") {
        if (b == 0.0) throw std::runtime_error("division by zero");
        return a / b;
    }

    const bool integers = std::holds_alternative<ZInteger>(left) &&
                          std::holds_alternative<ZInteger>(right);
    if (integers) {
        const ZInteger x = std::get<ZInteger>(left);
        const ZInteger y = std::get<ZInteger>(right);
        if (op == "+") return x + y;
        if (op == "-") return x - y;
        if (op == "*") return x * y;
    }
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    throw std::runtime_error("invalid operator");
}

static std::string zText(const ZValue& value)
{
    if (const auto* integer = std::get_if<ZInteger>(&value)) return std::to_string(*integer);
    if (const auto* floating = std::get_if<ZFloat>(&value)) {
        std::ostringstream output;
        output << std::setprecision(15) << *floating;
        return output.str();
    }
    if (const auto* text = std::get_if<ZString>(&value)) return *text;
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
    if (const auto* valueComplex = std::get_if<ZComplex>(&value)) {
        std::ostringstream output;
        output << valueComplex->real() << (valueComplex->imag() < 0 ? "" : "+")
               << valueComplex->imag() << "i";
        return output.str();
    }
    if (const auto* collection = std::get_if<ZVectorPtr>(&value)) {
        return (*collection)->isArray ? "<array>" : "<vector>";
    }
    return {};
}

static void zPrint(const ZValue& value) { std::cout << zText(value); }

static ZValue zToBox(const ZValue& value)
{
    if (std::holds_alternative<ZInteger>(value)) return value;
    if (const auto* floating = std::get_if<ZFloat>(&value)) {
        if (!std::isfinite(*floating) ||
            *floating < static_cast<double>(std::numeric_limits<ZInteger>::min()) ||
            *floating >= static_cast<double>(std::numeric_limits<ZInteger>::max()))
            throw std::runtime_error("cannot convert float to box: out of range");
        return ZInteger{static_cast<ZInteger>(*floating)};
    }
    if (const auto* text = std::get_if<ZString>(&value)) {
        try {
            std::size_t used = 0;
            const long long parsed = std::stoll(*text, &used);
            while (used < text->size() && std::isspace(static_cast<unsigned char>((*text)[used]))) ++used;
            if (used != text->size()) throw std::runtime_error("trailing characters");
            return ZInteger{static_cast<ZInteger>(parsed)};
        } catch (const std::exception&) {
            throw std::runtime_error("cannot convert input to box");
        }
    }
    throw std::runtime_error("cannot convert value to box");
}

static ZValue zToFloat(const ZValue& value)
{
    if (std::holds_alternative<ZFloat>(value)) return value;
    if (const auto* integer = std::get_if<ZInteger>(&value)) return ZFloat{static_cast<double>(*integer)};
    if (const auto* text = std::get_if<ZString>(&value)) {
        try {
            std::size_t used = 0;
            const double parsed = std::stod(*text, &used);
            while (used < text->size() && std::isspace(static_cast<unsigned char>((*text)[used]))) ++used;
            if (used != text->size() || !std::isfinite(parsed)) throw std::runtime_error("invalid float");
            return ZFloat{parsed};
        } catch (const std::exception&) {
            throw std::runtime_error("cannot convert input to float");
        }
    }
    throw std::runtime_error("cannot convert value to float");
}

static ZValue zTypeOf(const ZValue& value)
{
    if (std::holds_alternative<ZInteger>(value)) return ZString{"box"};
    if (std::holds_alternative<ZFloat>(value)) return ZString{"float"};
    if (std::holds_alternative<ZString>(value)) return ZString{"conda"};
    if (std::holds_alternative<bool>(value)) return ZString{"bool"};
    if (std::holds_alternative<ZComplex>(value)) return ZString{"complex"};
    if (const auto* collection = std::get_if<ZVectorPtr>(&value))
        return ZString{(*collection)->isArray ? "array" : "vector"};
    return ZString{"unknown"};
}

static ZValue zInput()
{
    std::string line;
    if (!std::getline(std::cin, line)) throw std::runtime_error("input failed");
    return ZString{std::move(line)};
}

static ZValue zInput(const ZValue& prompt)
{
    zPrint(prompt);
    std::cout.flush();
    return zInput();
}

)CPP";

    for (const auto& function : program.functions) {
        output_ += "static ZValue " + generatedFunctionName(*function) + "(";
        for (std::size_t index = 0; index < function->params.size(); ++index) {
            if (index != 0) output_ += ", ";
            output_ += "ZValue zusr_" + function->params[index];
        }
        output_ += ");\n";
    }

    for (const auto& function : program.functions) {
        output_ += "\nstatic ZValue " + generatedFunctionName(*function) + "(";
        for (std::size_t index = 0; index < function->params.size(); ++index) {
            if (index != 0) output_ += ", ";
            output_ += "ZValue zusr_" + function->params[index];
        }
        output_ += ")\n{\n";
        for (const auto& statement : function->body->statements) {
            generateStatement(*statement, 4);
        }
        output_ += "    return ZValue{};\n}\n";
    }

    output_ += R"CPP(
int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    try {
        zfn_main();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 5;
    }
}
)CPP";

    std::ofstream file(outputPath, std::ios::binary);
    file.write(output_.data(), static_cast<std::streamsize>(output_.size()));
    return static_cast<bool>(file);
}

} // namespace zlang
