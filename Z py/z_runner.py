import re

class ZInterpreter:
    def __init__(self):
        self.variables = {}
        self.functions = {}

    def run(self, code):
        # 주석 및 공백 정리
        lines = [line.strip() for line in code.split('\n') if line.strip() and not line.strip().startswith("//")]
        self.execute_block(lines)

    def execute_block(self, lines):
        i = 0
        while i < len(lines):
            line = lines[i]

            # 1. 함수 정의 (main 포함)
            if re.match(r'^\w+\s*\([^)]*\)\s*\{', line):
                func_name = line.split('(')[0].strip()
                block, next_i = self.extract_block(lines, i)
                self.functions[func_name] = block
                i = next_i
                continue

            # 2. 변수 선언 (box, conda, str, bool)
            var_match = re.match(r'^(box|conda|str|bool)\s+(\w+)\s*=\s*(.+)$', line)
            if var_match:
                vtype, vname, val_expr = var_match.groups()
                self.variables[vname] = self.eval_expr(val_expr)
                i += 1
                continue

            # 3. print.h 처리
            if line.startswith("print.h"):
                self.handle_print(line)
                i += 1
                continue

            # 4. if / else if / else 문
            if line.startswith("if"):
                condition_str = line[line.find("(")+1 : line.find(")")]
                block, next_i = self.extract_block(lines, i)
                
                if self.eval_expr(condition_str):
                    self.execute_block(block)
                    # if 조건이 맞았으므로 뒤따르는 else 문 skip
                    i = self.skip_else_blocks(lines, next_i)
                else:
                    i = next_i
                continue

            # 5. for i in +(n) 문
            for_match = re.match(r'^for\s+(\w+)\s+in\s+\+\(([^)]+)\)\s*\{', line)
            if for_match:
                var_name, range_arg = for_match.groups()
                range_val = int(self.eval_expr(range_arg))
                block, next_i = self.extract_block(lines, i)

                for val in range(range_val):
                    self.variables[var_name] = val
                    self.execute_block(block)

                i = next_i
                continue

            # 6. while 문
            while_match = re.match(r'^while\s*\(([^)]+)\)\s*\{', line)
            if while_match:
                cond_str = while_match.group(1)
                block, next_i = self.extract_block(lines, i)

                while self.eval_expr(cond_str):
                    self.execute_block(block)

                i = next_i
                continue

            # 7. 일반 변수 재할당 (e.g. i = i + 1)
            assign_match = re.match(r'^(\w+)\s*=\s*(.+)$', line)
            if assign_match:
                vname, val_expr = assign_match.groups()
                if vname in self.variables:
                    self.variables[vname] = self.eval_expr(val_expr)
                i += 1
                continue

            i += 1

    def extract_block(self, lines, start_i):
        """ 중괄호 { } 블록 내부 코드 추출 """
        block = []
        brace_count = 0
        started = False
        
        for idx in range(start_i, len(lines)):
            line = lines[idx]
            brace_count += line.count('{') - line.count('}')
            if '{' in line:
                started = True
            
            if started:
                # 첫 줄의 { 뒷부분이나 중간 줄 담기
                if idx != start_i and not (brace_count == 0 and '}' in line):
                    block.append(line)
            
            if started and brace_count == 0:
                return block, idx + 1
        return block, len(lines)

    def skip_else_blocks(self, lines, current_i):
        """ if문 조건 성립 시 else if / else 블록 건너뛰기 """
        while current_i < len(lines):
            line = lines[current_i]
            if line.startswith("else"):
                _, current_i = self.extract_block(lines, current_i)
            else:
                break
        return current_i

    def handle_print(self, line):
        """ print.h("...") 및 print.h "..." 모드 처리 """
        content = line[7:].strip()
        
        # 괄호 유무 파악
        if content.startswith("(") and content.endswith(")"):
            content = content[1:-1]
        
        # 반점으로 분할 후 출력 (간단 파싱)
        parts = [p.strip() for p in content.split(',')]
        output = ""
        for p in parts:
            val = self.eval_expr(p)
            output += str(val).replace("\\n", "\n")
        
        print(output, end="")

    def eval_expr(self, expr):
        """ 값 및 간단 연산 평가 """
        expr = expr.strip()
        if expr.startswith('"') and expr.endswith('"'):
            return expr[1:-1]
        if expr == "true": return True
        if expr == "false": return False
        
        # 심볼 테이블 적용
        local_env = {**self.variables}
        try:
            return eval(expr, {}, local_env)
        except:
            return expr

# --- 실행 테스트 ---
z_code = """
main() {
    box n = 10
    conda pi = 3.14
    str name = "철수"

    print.h(name, "님 안녕하세요\\n")
    print.h("pi: ", pi, "\\n")

    box score = 85
    if (score >= 90) {
        print.h "A학점입니다.\\n"
    } else if (score >= 80) {
        print.h "B학점입니다.\\n"
    } else {
        print.h "C학점입니다.\\n"
    }

    for i in +(3) {
        print.h i, " "
    }
    print.h "\\n"

    box i = 0
    while (i < 2) {
        print.h "while: ", i, "\\n"
        i = i + 1
    }
}
"""
# z_runner.py 맨 아래 부분 수정
if __name__ == "__main__":
    # main.z 파일 읽어오기
    with open("main.z", "r", encoding="utf-8") as f:
        z_code = f.read()

    interpreter = ZInterpreter()
    interpreter.run(z_code)

    if "main" in interpreter.functions:
        interpreter.execute_block(interpreter.functions["main"])