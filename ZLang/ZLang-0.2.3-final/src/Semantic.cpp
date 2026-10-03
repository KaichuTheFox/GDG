#include "zlang/Semantic.h"

namespace zlang {
namespace {

bool isNumeric(Type type) { return type == Type::Box || type == Type::Float; }

bool expressionMatches(Type expected, Type actual, const Expr& expression)
{
    return expected == actual || expected == Type::Unknown || actual == Type::Unknown ||
           (expected == Type::Array && actual == Type::Vector &&
            dynamic_cast<const VectorLiteralExpr*>(&expression) != nullptr);
}

bool literalMatches(const Expr& expression, const TypeSpec& expected)
{
    if (expected.kind != Type::Vector && expected.kind != Type::Array) {
        return expression.inferred == expected.kind || expression.inferred == Type::Unknown;
    }
    const auto* vector = dynamic_cast<const VectorLiteralExpr*>(&expression);
    if (!vector) {
        return expression.inferred == expected.kind || expression.inferred == Type::Unknown;
    }
    if (!expected.element) return false;
    if (expected.kind == Type::Array && vector->elements.size() != expected.arraySize) return false;
    for (const auto& element : vector->elements) {
        if (!literalMatches(*element, *expected.element)) return false;
    }
    return true;
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(Diagnostics& diagnostics) : diagnostics_(diagnostics) {}

void SemanticAnalyzer::pushScope() { scopes_.emplace_back(); }
void SemanticAnalyzer::popScope() { scopes_.pop_back(); }

std::optional<SemanticAnalyzer::VariableSymbol>
SemanticAnalyzer::findVariable(const std::string& name) const
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto symbol = scope->find(name);
        if (symbol != scope->end()) {
            return symbol->second;
        }
    }
    return std::nullopt;
}

std::optional<TypeSpec> SemanticAnalyzer::typeSpecOf(const Expr& expression) const
{
    if (const auto* name = dynamic_cast<const NameExpr*>(&expression)) {
        const auto variable = findVariable(name->name);
        if (variable) return variable->spec;
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(&expression)) {
        const auto collection = typeSpecOf(*index->collection);
        if (collection && (collection->kind == Type::Vector || collection->kind == Type::Array) &&
            collection->element)
            return *collection->element;
    }
    if (const auto* member = dynamic_cast<const MemberCallExpr*>(&expression)) {
        const auto collection = typeSpecOf(*member->object);
        if (member->member == "pop" && collection && collection->element) {
            return *collection->element;
        }
    }
    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        const auto function = functions_.find(call->resolvedCallee.empty()
                                                  ? call->callee
                                                  : call->resolvedCallee);
        if (function != functions_.end() && function->second->hasReturnType) {
            return function->second->returnSpec;
        }
    }
    return std::nullopt;
}

bool SemanticAnalyzer::typesCompatible(Type expected, Type actual) const
{
    return expected == actual || expected == Type::Unknown || actual == Type::Unknown;
}

std::string SemanticAnalyzer::functionKey(const Function& function) const
{
    return function.module.empty() ? function.name : function.module + "." + function.name;
}

bool SemanticAnalyzer::analyze(Program& program)
{
    int mainCount = 0;

    for (auto& function : program.functions) {
        const std::string key = functionKey(*function);
        if (functions_.count(key) != 0) {
            diagnostics_.error(function->span, "duplicate function '" + key + "'");
        } else {
            functions_[key] = function.get();
        }

        if (function->module.empty() && function->name == "main") {
            ++mainCount;
            if (!function->params.empty()) {
                diagnostics_.error(function->span, "main cannot have parameters");
            }
        }
    }

    if (mainCount == 0) {
        diagnostics_.error({}, "program requires exactly one main function");
    }
    if (mainCount > 1) {
        diagnostics_.error({}, "program must not define more than one main function");
    }

    // Revisit function bodies so calls can observe return types discovered later in the file.
    for (std::size_t pass = 0; pass < program.functions.size() + 1; ++pass) {
        for (auto& function : program.functions) {
            currentFunction_ = function.get();
            pushScope();
            for (std::size_t index = 0; index < function->params.size(); ++index) {
                TypeSpec parameterType;
                if (index < function->paramTypes.size()) parameterType = function->paramTypes[index];
                if (parameterType.kind == Type::Void) {
                    diagnostics_.error(function->span, "function parameter cannot have type void");
                }
                scopes_.back()[function->params[index]] =
                    {parameterType.kind, false, parameterType};
            }

            Type returnType = function->hasReturnType ? function->returnSpec.kind : Type::Unknown;
            for (auto& statement : function->body->statements) {
                analyzeStatement(*statement, returnType);
            }
            function->returnType = returnType;
            popScope();
        }
    }

    return !diagnostics_.hasErrors();
}

void SemanticAnalyzer::analyzeStatement(Stmt& statement, Type& returnType)
{
    if (auto* declaration = dynamic_cast<VarDecl*>(&statement)) {
        for (const auto& scope : scopes_) {
            if (scope.count(declaration->name) != 0) {
                diagnostics_.error(declaration->span,
                                   "duplicate or shadowed variable '" + declaration->name + "'");
                return;
            }
        }
        const Type initialType = analyzeExpression(*declaration->init);
        if (!expressionMatches(declaration->type.kind, initialType, *declaration->init) ||
            ((declaration->type.kind == Type::Vector || declaration->type.kind == Type::Array) &&
             !literalMatches(*declaration->init, declaration->type))) {
            diagnostics_.error(declaration->span,
                               "initializer type does not match variable type");
        }
        scopes_.back()[declaration->name] = {declaration->type.kind, false, declaration->type};
        return;
    }

    if (auto* assignment = dynamic_cast<Assign*>(&statement)) {
        const auto variable = findVariable(assignment->name);
        if (!variable) {
            diagnostics_.error(assignment->span,
                               "use of undeclared variable '" + assignment->name + "'");
        } else if (variable->isReadOnly) {
            diagnostics_.error(assignment->span, "cannot assign to read-only for variable");
        } else if (!expressionMatches(variable->type, analyzeExpression(*assignment->value),
                                      *assignment->value) ||
                   ((variable->type == Type::Vector || variable->type == Type::Array) &&
                    !literalMatches(*assignment->value, variable->spec))) {
            diagnostics_.error(assignment->span, "assignment type mismatch");
        }
        return;
    }

    if (auto* assignment = dynamic_cast<IndexAssign*>(&statement)) {
        const Type targetType = analyzeExpression(*assignment->target);
        const Type valueType = analyzeExpression(*assignment->value);
        if (targetType != Type::Unknown &&
            !expressionMatches(targetType, valueType, *assignment->value)) {
            diagnostics_.error(assignment->span, "vector element assignment type mismatch");
        }
        const auto targetSpec = typeSpecOf(*assignment->target);
        if (targetSpec && (targetSpec->kind == Type::Vector || targetSpec->kind == Type::Array) &&
            !literalMatches(*assignment->value, *targetSpec)) {
            diagnostics_.error(assignment->span, "vector element assignment type mismatch");
        }
        return;
    }

    if (auto* print = dynamic_cast<Print*>(&statement)) {
        for (auto& argument : print->args) {
            analyzeExpression(*argument);
        }
        return;
    }

    if (auto* conditional = dynamic_cast<If*>(&statement)) {
        if (analyzeExpression(*conditional->cond) != Type::Bool) {
            diagnostics_.error(conditional->cond->span, "if condition must be bool");
        }
        pushScope();
        for (auto& child : conditional->thenBlock->statements) {
            analyzeStatement(*child, returnType);
        }
        popScope();

        for (auto& branch : conditional->elseIfs) {
            if (analyzeExpression(*branch.first) != Type::Bool) {
                diagnostics_.error(branch.first->span, "else-if condition must be bool");
            }
            pushScope();
            for (auto& child : branch.second->statements) {
                analyzeStatement(*child, returnType);
            }
            popScope();
        }

        if (conditional->elseBlock) {
            pushScope();
            for (auto& child : conditional->elseBlock->statements) {
                analyzeStatement(*child, returnType);
            }
            popScope();
        }
        return;
    }

    if (auto* loop = dynamic_cast<While*>(&statement)) {
        if (analyzeExpression(*loop->cond) != Type::Bool) {
            diagnostics_.error(loop->cond->span, "while condition must be bool");
        }
        pushScope();
        ++loopDepth_;
        for (auto& child : loop->body->statements) {
            analyzeStatement(*child, returnType);
        }
        --loopDepth_;
        popScope();
        return;
    }

    if (auto* loop = dynamic_cast<For*>(&statement)) {
        for (Expr* part : {loop->start.get(), loop->end.get(), loop->step.get()}) {
            if (part && analyzeExpression(*part) != Type::Box) {
                diagnostics_.error(part->span, "for range values must be box integers");
            }
        }
        pushScope();
        ++loopDepth_;
        scopes_.back()[loop->name] = {Type::Box, true, {}};
        for (auto& child : loop->body->statements) {
            analyzeStatement(*child, returnType);
        }
        --loopDepth_;
        popScope();
        return;
    }

    if (dynamic_cast<Break*>(&statement)) {
        if (loopDepth_ == 0) {
            diagnostics_.error(statement.span, "brk can only be used inside a loop");
        }
        return;
    }

    if (dynamic_cast<Continue*>(&statement)) {
        if (loopDepth_ == 0) {
            diagnostics_.error(statement.span, "continue can only be used inside a loop");
        }
        return;
    }

    if (auto* returnStatement = dynamic_cast<Return*>(&statement)) {
        const Type discovered = returnStatement->value
                                    ? analyzeExpression(*returnStatement->value)
                                    : Type::Void;
        if (currentFunction_->hasReturnType && returnStatement->value &&
            !literalMatches(*returnStatement->value, currentFunction_->returnSpec)) {
            diagnostics_.error(returnStatement->span, "return value does not match declared type");
        }
        if (returnType == Type::Unknown) {
            returnType = discovered;
        } else if (!typesCompatible(returnType, discovered)) {
            diagnostics_.error(returnStatement->span, "incompatible function return types");
        }
        return;
    }

    if (auto* expressionStatement = dynamic_cast<ExprStmt*>(&statement)) {
        analyzeExpression(*expressionStatement->expr, false);
    }
}

Type SemanticAnalyzer::analyzeExpression(Expr& expression, bool valueRequired)
{
    Type result = Type::Unknown;

    if (auto* literal = dynamic_cast<LiteralExpr*>(&expression)) {
        if (std::holds_alternative<std::string>(literal->value)) {
            result = Type::Conda;
        } else if (std::holds_alternative<bool>(literal->value)) {
            result = Type::Bool;
        } else if (std::holds_alternative<double>(literal->value)) {
            result = Type::Float;
        } else {
            result = Type::Box;
        }
    } else if (auto* name = dynamic_cast<NameExpr*>(&expression)) {
        const auto variable = findVariable(name->name);
        if (!variable) {
            diagnostics_.error(name->span, "use of undeclared variable '" + name->name + "'");
        } else {
            result = variable->type;
        }
    } else if (auto* unary = dynamic_cast<UnaryExpr*>(&expression)) {
        result = analyzeExpression(*unary->rhs);
        if (unary->op == "!") {
            if (result != Type::Bool && result != Type::Unknown) {
                diagnostics_.error(unary->span, "logical '!' requires a bool operand");
            }
            result = Type::Bool;
        } else if (!isNumeric(result) && result != Type::Unknown) {
            diagnostics_.error(unary->span, "unary operator requires box or float");
        }
    } else if (auto* binary = dynamic_cast<BinaryExpr*>(&expression)) {
        const Type left = analyzeExpression(*binary->lhs);
        const Type right = analyzeExpression(*binary->rhs);

        if (binary->op == "&&") {
            if ((left != Type::Bool && left != Type::Unknown) ||
                (right != Type::Bool && right != Type::Unknown)) {
                diagnostics_.error(binary->span, "logical '&&' requires bool operands");
            }
            result = Type::Bool;
        } else if (binary->op == "or") {
            if ((left != Type::Bool && left != Type::Unknown) ||
                (right != Type::Bool && right != Type::Unknown)) {
                diagnostics_.error(binary->span, "logical 'or' requires bool operands");
            }
            result = Type::Bool;
        } else if (binary->op == "%") {
            if ((left != Type::Box && left != Type::Unknown) ||
                (right != Type::Box && right != Type::Unknown)) {
                diagnostics_.error(binary->span, "'%' requires box integer operands");
            }
            result = left == Type::Unknown || right == Type::Unknown ? Type::Unknown : Type::Box;
        } else if (binary->op == "==" || binary->op == "!=") {
            if (!typesCompatible(left, right)) {
                diagnostics_.error(binary->span, "comparison type mismatch");
            }
            result = Type::Bool;
        } else if (binary->op == "<" || binary->op == "<=" || binary->op == ">" ||
                   binary->op == ">=") {
            if ((!isNumeric(left) && left != Type::Unknown) ||
                (!isNumeric(right) && right != Type::Unknown)) {
                diagnostics_.error(binary->span, "ordered comparison requires numeric values");
            }
            result = Type::Bool;
        } else if (left == Type::Unknown || right == Type::Unknown) {
            result = Type::Unknown;
        } else if (binary->op == "+" && left == Type::Conda && right == Type::Conda) {
            result = Type::Conda;
        } else if (left == Type::Complex && right == Type::Complex) {
            result = Type::Complex;
        } else if (isNumeric(left) && isNumeric(right)) {
            result = binary->op == "/" || left == Type::Float || right == Type::Float
                         ? Type::Float
                         : Type::Box;
        } else {
            diagnostics_.error(binary->span, "invalid operands for '" + binary->op + "'");
        }
    } else if (auto* call = dynamic_cast<CallExpr*>(&expression)) {
        if (call->callee == "input") {
            if (call->args.size() > 1) {
                diagnostics_.error(call->span, "input accepts zero or one prompt argument");
            }
            if (!call->args.empty() && analyzeExpression(*call->args.front()) != Type::Conda) {
                diagnostics_.error(call->args.front()->span, "input prompt must be conda text");
            }
            result = Type::Conda;
        } else if (call->callee == "type") {
            if (call->args.size() != 1) {
                diagnostics_.error(call->span, "type requires exactly one argument");
            }
            for (auto& argument : call->args) analyzeExpression(*argument);
            result = Type::Conda;
        } else if (call->callee == "box" || call->callee == "float") {
            if (call->args.size() != 1) {
                diagnostics_.error(call->span, call->callee + " requires exactly one argument");
            }
            if (!call->args.empty()) {
                const Type source = analyzeExpression(*call->args.front());
                if (!isNumeric(source) && source != Type::Conda && source != Type::Unknown) {
                    diagnostics_.error(call->args.front()->span,
                                       call->callee + " conversion requires numeric or text input");
                }
            }
            result = call->callee == "box" ? Type::Box : Type::Float;
        } else if (call->callee == "complex") {
            if (call->args.empty() || call->args.size() > 2) {
                diagnostics_.error(call->span, "complex requires one or two numeric arguments");
            }
            for (auto& argument : call->args) {
                const Type argumentType = analyzeExpression(*argument);
                if (!isNumeric(argumentType) && argumentType != Type::Unknown) {
                    diagnostics_.error(argument->span, "complex arguments must be numeric");
                }
            }
            result = Type::Complex;
        } else {
            std::string key = call->callee;
            if (key.find('.') == std::string::npos && !currentFunction_->module.empty()) {
                key = currentFunction_->module + "." + key;
            }
            auto function = functions_.find(key);
            if (function == functions_.end() && key.find('.') == std::string::npos) {
                function = functions_.find(call->callee);
                if (function != functions_.end()) key = call->callee;
            }
            if (function == functions_.end()) {
                diagnostics_.error(call->span, "unknown function '" + call->callee + "'");
            } else {
                call->resolvedCallee = key;
                if (call->args.size() != function->second->params.size()) {
                    diagnostics_.error(call->span, "function argument count mismatch");
                }
                for (std::size_t index = 0; index < call->args.size(); ++index) {
                    const Type actual = analyzeExpression(*call->args[index]);
                    if (index < function->second->paramTypes.size()) {
                        const TypeSpec& expected = function->second->paramTypes[index];
                        if (expected.kind != Type::Unknown &&
                            (!expressionMatches(expected.kind, actual, *call->args[index]) ||
                             !literalMatches(*call->args[index], expected))) {
                            diagnostics_.error(call->args[index]->span,
                                               "function argument type mismatch");
                        }
                    }
                }
                result = function->second->returnType;
            }
        }
    } else if (auto* vector = dynamic_cast<VectorLiteralExpr*>(&expression)) {
        for (auto& element : vector->elements) analyzeExpression(*element);
        result = Type::Vector;
    } else if (auto* index = dynamic_cast<IndexExpr*>(&expression)) {
        const Type collection = analyzeExpression(*index->collection);
        const Type subscript = analyzeExpression(*index->index);
        if (collection != Type::Vector && collection != Type::Array && collection != Type::Unknown) {
            diagnostics_.error(index->collection->span, "indexing requires a vector or array");
        }
        if (subscript != Type::Box && subscript != Type::Unknown) {
            diagnostics_.error(index->index->span, "vector index must be box");
        }
        const auto elementType = typeSpecOf(*index->collection);
        result = elementType &&
                         (elementType->kind == Type::Vector || elementType->kind == Type::Array) &&
                         elementType->element
                     ? elementType->element->kind
                     : (elementType ? elementType->kind : Type::Unknown);
    } else if (auto* member = dynamic_cast<MemberCallExpr*>(&expression)) {
        const Type object = analyzeExpression(*member->object);
        std::vector<Type> argumentTypes;
        for (auto& argument : member->args) argumentTypes.push_back(analyzeExpression(*argument));
        if (member->member == "size" && member->args.empty() &&
            (object == Type::Vector || object == Type::Array || object == Type::Unknown)) {
            result = Type::Box;
        } else if (member->member == "push") {
            if (object != Type::Vector && object != Type::Unknown) {
                diagnostics_.error(member->span, "push can only be used on a vector");
            }
            if (member->args.size() != 1) {
                diagnostics_.error(member->span, "push requires exactly one argument");
            } else if (const auto spec = typeSpecOf(*member->object); spec && spec->element &&
                       (!expressionMatches(spec->element->kind, argumentTypes.front(),
                                           *member->args.front()) ||
                        !literalMatches(*member->args.front(), *spec->element))) {
                diagnostics_.error(member->args.front()->span, "vector element type mismatch");
            }
            result = Type::Void;
        } else if (member->member == "pop" && member->args.empty()) {
            if (object != Type::Vector && object != Type::Unknown) {
                diagnostics_.error(member->span, "pop can only be used on a vector");
            }
            const auto spec = typeSpecOf(*member->object);
            result = spec && spec->element ? spec->element->kind : Type::Unknown;
        } else {
            diagnostics_.error(member->span, "supported vector methods are .size(), .push(), and .pop()");
        }
    }

    if (valueRequired && result == Type::Void) {
        diagnostics_.error(expression.span, "void function result used as a value");
    }
    expression.inferred = result;
    return result;
}

} // namespace zlang
