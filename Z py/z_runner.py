import re

# ==========================================
# 1. LEXER
# ==========================================
TOKEN_TYPES = [
    ('NUMBER',   r'\d+(\.\d+)?'),
    ('STRING',   r'"[^"]*"'),
    ('COMMENT',  r'//[^\n]*'),
    ('KEYWORD',  r'\b(print|for|in|if|else|while|return|true|false)\b'),
    ('IDENT',    r'[a-zA-Z_][a-zA-Z0-9_]*'),
    ('PLUS_RNG', r'\+\('),
    ('OP',       r'==|!=|<=|>=|\+|-|\*|/|<|>|='),
    ('LPAREN',   r'\('),
    ('RPAREN',   r'\)'),
    ('LBRACE',   r'\{'),
    ('RBRACE',   r'\}'),
    ('COMMA',    r','),
    ('SKIP',     r'[ \t\n]+'),
    ('MISMATCH', r'.'),
]

class Token:
    def __init__(self, type_, value):
        self.type = type_
        self.value = value
    def __repr__(self):
        return f"Token({self.type}, {self.value})"

def lexer(code):
    tokens = []
    tok_regex = '|'.join(f'(?P<{pair[0]}>{pair[1]})' for pair in TOKEN_TYPES)
    for mo in re.finditer(tok_regex, code):
        kind = mo.lastgroup
        value = mo.group()
        if kind in ('SKIP', 'COMMENT'):
            continue
        elif kind == 'MISMATCH':
            raise SyntaxError(f"Syntax Error: {value}")
        tokens.append(Token(kind, value))
    return tokens

# ==========================================
# 2. PARSER & AST
# ==========================================
class ASTNode: pass
class ProgramNode(ASTNode):
    def __init__(self, funcs): self.funcs = funcs
class FuncDeclNode(ASTNode):
    def __init__(self, name, params, body):
        self.name, self.params, self.body = name, params, body
class VarDeclNode(ASTNode):
    def __init__(self, vtype, name, expr):
        self.vtype, self.name, self.expr = vtype, name, expr
class AssignNode(ASTNode):
    def __init__(self, name, expr): self.name, self.expr = name, expr
class PrintNode(ASTNode):
    def __init__(self, args): self.args = args
class IfNode(ASTNode):
    def __init__(self, cond, then_b, else_b):
        self.cond, self.then_b, self.else_b = cond, then_b, else_b
class ForNode(ASTNode):
    def __init__(self, var, range_expr, body):
        self.var, self.range_expr, self.body = var, range_expr, body
class WhileNode(ASTNode):
    def __init__(self, cond, body): self.cond, self.body = cond, body
class ReturnNode(ASTNode):
    def __init__(self, expr): self.expr = expr
class FuncCallNode(ASTNode):
    def __init__(self, name, args): self.name, self.args = name, args
class BinaryOpNode(ASTNode):
    def __init__(self, left, op, right): self.left, self.op, self.right = left, op, right
class LiteralNode(ASTNode):
    def __init__(self, val): self.val = val
class VarRefNode(ASTNode):
    def __init__(self, name): self.name = name

class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def peek(self, offset=0):
        idx = self.pos + offset
        return self.tokens[idx] if idx < len(self.tokens) else Token('EOF', '')

    def consume(self, type_=None, val=None):
        tok = self.peek()
        if type_ and tok.type != type_:
            raise SyntaxError(f"Unexpected token: {tok.value} (Type: {tok.type}, Expected: {type_})")
        if val and tok.value != val:
            raise SyntaxError(f"Unexpected value: {tok.value} (Expected: {val})")
        self.pos += 1
        return tok

    def parse(self):
        funcs = []
        while self.peek().type != 'EOF':
            funcs.append(self.parse_function())
        return ProgramNode(funcs)

    def parse_function(self):
        name = self.consume('IDENT').value
        self.consume('LPAREN')
        params = []
        if self.peek().type != 'RPAREN':
            params.append(self.consume('IDENT').value)
            while self.peek().type == 'COMMA':
                self.consume('COMMA')
                params.append(self.consume('IDENT').value)
        self.consume('RPAREN')
        body = self.parse_block()
        return FuncDeclNode(name, params, body)

    def parse_block(self):
        self.consume('LBRACE')
        stmts = []
        while self.peek().type != 'RBRACE' and self.peek().type != 'EOF':
            stmts.append(self.parse_stmt())
        self.consume('RBRACE')
        return stmts

    def parse_stmt(self):
        tok = self.peek()
        
        if tok.type == 'IDENT' and tok.value in ('box', 'conda', 'line', 'bool', 'cpx'):
            vtype = self.consume('IDENT').value
            name = self.consume('IDENT').value
            self.consume('OP', '=')
            expr = self.parse_expr()
            return VarDeclNode(vtype, name, expr)
        
        elif tok.value == 'print':
            self.consume()
            has_paren = False
            if self.peek().type == 'LPAREN':
                has_paren = True
                self.consume('LPAREN')
            
            args = [self.parse_expr()]
            while self.peek().type == 'COMMA':
                self.consume('COMMA')
                args.append(self.parse_expr())
            
            if has_paren:
                self.consume('RPAREN')
            return PrintNode(args)

        elif tok.value == 'if':
            self.consume('KEYWORD', 'if')
            self.consume('LPAREN')
            cond = self.parse_expr()
            self.consume('RPAREN')
            then_b = self.parse_block()
            else_b = None
            if self.peek().value == 'else':
                self.consume()
                if self.peek().value == 'if':
                    else_b = [self.parse_stmt()]
                else:
                    else_b = self.parse_block()
            return IfNode(cond, then_b, else_b)

        elif tok.value == 'for':
            self.consume('KEYWORD', 'for')
            var_name = self.consume('IDENT').value
            self.consume('KEYWORD', 'in')
            self.consume('PLUS_RNG')
            range_expr = self.parse_expr()
            self.consume('RPAREN')
            body = self.parse_block()
            return ForNode(var_name, range_expr, body)

        elif tok.value == 'while':
            self.consume('KEYWORD', 'while')
            self.consume('LPAREN')
            cond = self.parse_expr()
            self.consume('RPAREN')
            body = self.parse_block()
            return WhileNode(cond, body)

        elif tok.value == 'return':
            self.consume()
            expr = self.parse_expr()
            return ReturnNode(expr)

        elif tok.type == 'IDENT' and self.peek(1).value == '=':
            name = self.consume('IDENT').value
            self.consume('OP', '=')
            expr = self.parse_expr()
            return AssignNode(name, expr)

        else:
            return self.parse_expr()

    def parse_expr(self):
        return self.parse_equality()

    def parse_equality(self):
        expr = self.parse_additive()
        while self.peek().type == 'OP' and self.peek().value in ('==', '!=', '<', '>', '<=', '>='):
            op = self.consume('OP').value
            right = self.parse_additive()
            expr = BinaryOpNode(expr, op, right)
        return expr

    def parse_additive(self):
        expr = self.parse_term()
        while self.peek().type == 'OP' and self.peek().value in ('+', '-'):
            op = self.consume('OP').value
            right = self.parse_term()
            expr = BinaryOpNode(expr, op, right)
        return expr

    def parse_term(self):
        expr = self.parse_primary()
        while self.peek().type == 'OP' and self.peek().value in ('*', '/'):
            op = self.consume('OP').value
            right = self.parse_primary()
            expr = BinaryOpNode(expr, op, right)
        return expr

    def parse_primary(self):
        tok = self.peek()
        if tok.type == 'NUMBER':
            self.consume()
            val = float(tok.value) if '.' in tok.value else int(tok.value)
            return LiteralNode(val)
        elif tok.type == 'STRING':
            self.consume()
            return LiteralNode(tok.value[1:-1])
        elif tok.value in ('true', 'false'):
            self.consume()
            return LiteralNode(tok.value == 'true')
        elif tok.type == 'IDENT':
            name = self.consume().value
            if self.peek().type == 'LPAREN':
                self.consume('LPAREN')
                args = []
                if self.peek().type != 'RPAREN':
                    args.append(self.parse_expr())
                    while self.peek().type == 'COMMA':
                        self.consume('COMMA')
                        args.append(self.parse_expr())
                self.consume('RPAREN')
                return FuncCallNode(name, args)
            return VarRefNode(name)
        elif tok.type == 'LPAREN':
            self.consume('LPAREN')
            expr = self.parse_expr()
            self.consume('RPAREN')
            return expr
            
        raise SyntaxError(f"Expression Parse Error: {tok.value}")

# ==========================================
# 3. EVALUATOR
# ==========================================
class ReturnException(Exception):
    def __init__(self, value): self.value = value

class Evaluator:
    def __init__(self):
        self.global_env = {}
        self.functions = {}

    def eval_program(self, node):
        for func in node.funcs:
            self.functions[func.name] = func
        if 'main' in self.functions:
            self.call_func('main', [])

    def call_func(self, name, args):
        if name == 'cpx':
            return complex(args[0], args[1])
        
        func = self.functions[name]
        env = {param: val for param, val in zip(func.params, args)}
        
        try:
            self.eval_stmts(func.body, env)
        except ReturnException as ret:
            return ret.value
        return None

    def eval_stmts(self, stmts, env):
        for stmt in stmts:
            self.eval_stmt(stmt, env)

    def eval_stmt(self, stmt, env):
        if isinstance(stmt, VarDeclNode):
            env[stmt.name] = self.eval_expr(stmt.expr, env)
        elif isinstance(stmt, AssignNode):
            env[stmt.name] = self.eval_expr(stmt.expr, env)
        elif isinstance(stmt, PrintNode):
            output = ""
            for arg in stmt.args:
                val = self.eval_expr(arg, env)
                output += self.format_val(val)
            print(output, end="")
        elif isinstance(stmt, IfNode):
            cond = self.eval_expr(stmt.cond, env)
            if cond:
                self.eval_stmts(stmt.then_b, env)
            elif stmt.else_b:
                self.eval_stmts(stmt.else_b, env)
        elif isinstance(stmt, ForNode):
            r_val = int(self.eval_expr(stmt.range_expr, env))
            for val in range(r_val):
                env[stmt.var] = val
                self.eval_stmts(stmt.body, env)
        elif isinstance(stmt, WhileNode):
            while self.eval_expr(stmt.cond, env):
                self.eval_stmts(stmt.body, env)
        elif isinstance(stmt, ReturnNode):
            val = self.eval_expr(stmt.expr, env)
            raise ReturnException(val)
        else:
            self.eval_expr(stmt, env)

    def eval_expr(self, expr, env):
        if isinstance(expr, LiteralNode):
            return expr.val
        elif isinstance(expr, VarRefNode):
            if expr.name in env: return env[expr.name]
            if expr.name in self.global_env: return self.global_env[expr.name]
            raise NameError(f"Undefined Variable: {expr.name}")
        elif isinstance(expr, BinaryOpNode):
            l = self.eval_expr(expr.left, env)
            r = self.eval_expr(expr.right, env)
            op = expr.op
            if op == '+': return l + r
            elif op == '-': return l - r
            elif op == '*': return l * r
            elif op == '/': return l / r
            elif op == '==': return l == r
            elif op == '!=': return l != r
            elif op == '<': return l < r
            elif op == '>': return l > r
            elif op == '<=': return l <= r
            elif op == '>=': return l >= r
        elif isinstance(expr, FuncCallNode):
            args = [self.eval_expr(a, env) for a in expr.args]
            return self.call_func(expr.name, args)

    def format_val(self, val):
        if isinstance(val, complex):
            real = int(val.real) if val.real.is_integer() else val.real
            imag = int(val.imag) if val.imag.is_integer() else val.imag
            sign = "+" if imag >= 0 else "-"
            return f"{real} {sign} {abs(imag)}i"
        return str(val).replace("\\n", "\n")

# ==========================================
# 4. EXECUTION
# ==========================================
if __name__ == "__main__":
    try:
        with open("main.z", "r", encoding="utf-8") as f:
            code = f.read()

        tokens = lexer(code)
        parser = Parser(tokens)
        ast = parser.parse()
        
        evaluator = Evaluator()
        evaluator.eval_program(ast)

    except FileNotFoundError:
        print("Error: main.z file not found.")
    except Exception as e:
        print(f"Execution Error: {e}")