# Z 언어 문법 안내서

이 문서는 `src/Lexer.cpp`, `src/Parser.cpp`, `src/Semantic.cpp`, `src/CodeGen.cpp`의 현재 구현을 기준으로 합니다. 별도의 문법 생성기나 `.g` 문법 파일은 없습니다. 실제 파싱 규칙은 `Parser.cpp`에 구현되어 있습니다.

## 1. 프로그램과 함수

프로그램은 함수 선언들로 구성됩니다. 프로그램 전체에 `main()`이 정확히 하나 있어야 하고, `main`에는 매개변수를 쓸 수 없습니다. 다른 함수의 매개변수에는 이름만 적으며 타입을 쓰지 않습니다.

```z
add(a, b) {
    return a + b
}

main() {
    box result = add(3, 4)
    print("결과: ", result, "\n")
}
```

함수 반환 타입은 선언하지 않습니다. `return`에서 반환한 식을 바탕으로 컴파일러가 타입을 추론합니다. 반환하지 않는 함수는 반환값 없이 호출할 수 있습니다.

## 2. 문장 문법

대략적인 문법을 EBNF로 표현하면 다음과 같습니다.

```text
program       = { function } ;
function      = ( "main" | identifier ), "(", [ parameters ], ")", block ;
parameters    = identifier, { ",", identifier } ;
block         = "{", { statement }, "}" ;

statement     = declaration
              | assignment
              | print
              | if_statement
              | while_statement
              | for_statement
              | return_statement
              | expression ;

declaration   = type, identifier, "=", expression ;
type          = "box" | "line" | "bool" | "cpx" ;
assignment    = identifier, "=", expression ;
print         = "print", [ [ "(" ], expression_list, [ ")" ] ] ;
if_statement  = "if", "(", expression, ")", block,
                { "else", "if", "(", expression, ")", block },
                [ "else", block ] ;
while_statement = "while", "(", expression, ")", block ;
for_statement = "for", identifier, "in", "+(", expression, ")", block ;
return_statement = "return", [ expression ] ;
expression_list = expression, { ",", expression } ;
```

선언은 초기값이 반드시 필요합니다. 문장은 줄바꿈이나 세미콜론으로 구분할 수 있습니다. 괄호 안의 줄바꿈은 무시됩니다. `//`부터 줄 끝까지는 주석입니다.

## 3. 변수와 타입

| 타입 | 용도 | 예 |
| --- | --- | --- |
| `box` | 정수 또는 실수 | `box count = 3`, `box rate = 1.5` |
| `line` | 문자열 | `line name = "Z"` |
| `bool` | 참/거짓 | `bool ready = true` |
| `cpx` | 복소수 | `cpx value = cpx(3, 4)` |

변수는 선언된 함수와 블록 범위에서 사용할 수 있습니다. 같은 이름을 다시 선언하거나 바깥 범위의 변수 이름을 가리는 선언은 허용되지 않습니다. `for` 반복 변수는 반복문 안에서만 보이며 읽기 전용입니다.

`cpx(real, imaginary)`는 복소수를 만드는 내장 함수이며 인수 두 개는 `box`여야 합니다.

## 4. 식과 연산자

지원하는 리터럴은 정수, 소수, 문자열, `true`, `false`입니다. 숫자는 `12`, `3.5`처럼 씁니다. 소수점 뒤에는 숫자가 필요하고, 지수 표기법은 지원하지 않습니다.

연산자 우선순위는 높은 순서부터 다음과 같습니다.

| 순위 | 연산자 | 결합 방향 |
| --- | --- | --- |
| 1 | 함수 호출 `f(...)`, 괄호 `(식)` | 왼쪽부터 |
| 2 | 단항 `+`, `-` | 오른쪽부터 |
| 3 | `*`, `/` | 왼쪽부터 |
| 4 | `+`, `-` | 왼쪽부터 |
| 5 | `<`, `<=`, `>`, `>=` | 왼쪽부터 |
| 6 | `==`, `!=` | 왼쪽부터 |

예를 들어 `2 + 3 * 4`는 `2 + (3 * 4)`로 계산합니다.

주요 타입 규칙:

- `box`끼리 `+`, `-`, `*`, `/`를 할 수 있습니다. `/` 결과는 실수일 수 있습니다.
- `line + line`은 문자열을 이어 붙입니다.
- `cpx`끼리 `+`, `-`, `*`, `/`를 할 수 있습니다.
- `==`, `!=`는 같은 타입 값끼리 비교합니다.
- `<`, `<=`, `>`, `>=`는 `box` 값끼리만 비교할 수 있습니다.
- 단항 `+`, `-`는 `box`에 사용할 수 있습니다.
- `if`와 `while` 조건은 `bool`이어야 합니다.

## 5. 출력과 제어 흐름

`print`는 인수들을 순서대로 출력합니다. 인수 사이에 공백이나 줄바꿈을 자동으로 넣지 않습니다. 괄호를 쓰거나 생략할 수 있습니다.

```z
print("Hello", " ", "World!\n")
print "값: ", 10, "\n"
```

문자열 이스케이프는 `\n`, `\r`, `\t`, `\\`, `\"`, `\0`입니다. 문자열은 한 줄 안에서 닫아야 합니다.

```z
if (score >= 60) {
    print("통과\n")
} else {
    print("다시 도전\n")
}

while (count > 0) {
    count = count - 1
}

for i in +(3) {
    print(i, " ")
}
```

범위 반복은 항상 0부터 시작하고 끝값은 포함하지 않습니다. 위 예제에서 `i`는 0, 1, 2입니다. 끝값은 음이 아닌 정수여야 합니다.

## 6. 현재 지원하지 않는 문법과 주의점

현재 구현에는 논리 연산자(`&&`, `||`, `!`), 나머지 연산자(`%`), `break`, `continue`, 배열, 클래스, 모듈, 사용자 정의 타입 선언이 없습니다. 복합 대입(`+=`)과 대입식을 식 안에 넣는 기능도 없습니다.

함수 매개변수 타입은 선언할 수 없고, 반환 타입 추론도 단순합니다. 특히 다른 함수의 반환 타입을 호출 순서와 관계없이 완전하게 추론하는 기능은 구현되어 있지 않으므로, 반환 타입이 중요한 함수는 먼저 선언하거나 사용을 단순하게 유지하는 편이 안전합니다.

실행은 다음 순서로 진행됩니다: 렉싱 → 파싱 및 AST 생성 → 의미 분석 → C++17 생성 → 네이티브 컴파일 → 실행. 실행 중 0으로 나누기 같은 오류는 `runtime error`로 보고됩니다.
