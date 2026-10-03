# Z 언어 문법 안내서

이 문서는 `src/Lexer.cpp`, `src/Parser.cpp`, `src/Semantic.cpp`, `src/CodeGen.cpp`의 현재 구현을 기준으로 합니다. 별도의 문법 생성기나 `.g` 문법 파일은 없습니다. 실제 파싱 규칙은 `Parser.cpp`에 구현되어 있습니다.

## 1. 프로그램과 함수

프로그램은 `import` 선언과 함수 선언으로 구성됩니다. 실행을 시작한 파일에 매개변수 없는 `main()`이 하나 있어야 합니다. 가져온 모듈은 `main()`을 정의할 수 없습니다.

```z
import math_tools

main() {
    box rounded = math_tools.roundDown(7.8)
    print("내림 결과: ", rounded, "\n")
}
```

같은 폴더의 `math_tools.z`에는 다음처럼 함수를 작성합니다.

```z
box roundDown(float value) {
    return box(value)
}
```

`import` 뒤에는 가져올 파일의 확장자를 뺀 이름을 씁니다. 예를 들어 `import math_tools`는 현재 `.z` 파일과 같은 폴더의 `math_tools.z`를 가져옵니다. 파일 이름은 `math_tools`, `number`, `my_module2`처럼 Z 식별자 규칙에 맞으면 원하는 이름으로 정할 수 있습니다. 확장자는 `.z`여야 하며, 경로나 별칭은 아직 지원하지 않습니다. 모듈 함수는 `math_tools.roundDown(...)`처럼 파일 이름을 모듈 이름으로 붙여 호출합니다. 모듈 안의 함수는 같은 모듈의 다른 함수를 이름만으로 호출할 수 있습니다. 중첩 import도 가능하며, 순환 import는 오류로 처리됩니다.

예를 들어 `main.z`와 `math_tools.z`가 같은 폴더에 있을 때 다음처럼 작성합니다.

```z
// main.z
import math_tools

main() {
    print(math_tools.add(10, 20), "\\n")
}
```

```z
// math_tools.z
box add(box a, box b) {
    return a + b
}
```

함수 반환 타입은 생략할 수 있으며, 생략하면 `return` 식으로 타입을 추론합니다. `box add(box a, box b)`처럼 반환 타입과 매개변수 타입을 명시할 수도 있습니다. 반환값이 없는 함수는 `void greet()`처럼 선언하거나 기존처럼 반환 타입 없이 작성할 수 있습니다.

## 2. 문장 문법

대략적인 문법을 EBNF로 표현하면 다음과 같습니다.

```text
program       = { import_decl | function } ;
import_decl   = "import", module_name ;
module_name   = identifier ; (* 같은 폴더의 module_name.z를 가져옴 *)
function      = [ return_type ], ( "main" | identifier ), "(", [ parameters ], ")", block ;
parameters    = identifier, { ",", identifier }
              | type, identifier, { ",", type, identifier } ;
return_type   = type | "void" ;
block         = "{", { statement }, "}" ;

statement     = declaration
              | assignment
              | print
              | if_statement
              | while_statement
              | for_statement
              | return_statement
              | break_statement
              | continue_statement
              | expression ;

declaration   = type, identifier, "=", expression ;
type          = "box" | "float" | "conda" | "bool" | "complex"
              | "vector", "<", type, ">"
              | "array", "<", type, ",", integer, ">" ;
assignment    = identifier, "=", expression
              | postfix, "=", expression ;
postfix       = primary, { "[", expression, "]"
                         | ".", identifier, "(", [ expression_list ], ")" } ;
print         = "print", [ [ "(" ], expression_list, [ ")" ] ] ;
if_statement  = "if", "(", expression, ")", block,
                { "else", "if", "(", expression, ")", block },
                [ "else", block ] ;
while_statement = "while", "(", expression, ")", block ;
for_statement = "for", identifier, "in",
                ( "+(", expression, ")" | "range", "(", expression_list, ")" ), block ;
return_statement = "return", [ expression ] ;
break_statement = "brk" ;
continue_statement = "continue" ;
expression_list = expression, { ",", expression } ;
primary       = literal | identifier | "(", expression, ")"
              | "[", [ expression_list ], "]" ;
```

선언은 초기값이 반드시 필요합니다. 문장은 줄바꿈이나 세미콜론으로 구분할 수 있습니다. 괄호 안의 줄바꿈은 무시됩니다. `//`부터 줄 끝까지는 주석입니다.

## 3. 변수와 타입

| 타입 | 용도 | 예 |
| --- | --- | --- |
| `box` | 정수 | `box count = 3` |
| `float` | 실수 | `float rate = 1.5` |
| `conda` | 문자열 | `conda name = "Z"` |
| `bool` | 참/거짓 | `bool ready = true` |
| `complex` | 복소수 | `complex value = complex(3, 4)` |
| `vector<T>` | T 타입 값을 담는 가변 길이 벡터 | `vector<box> nums = [1, 2, 3]` |
| `array<T, N>` | T 타입 값을 N개 담는 고정 크기 배열 | `array<box, 3> nums = [1, 2, 3]` |

함수는 기존처럼 타입을 생략할 수도 있고, 반환 타입과 매개변수 타입을 적을 수도 있습니다. 반환 타입이 없는 함수에는 `void`를 쓸 수 있습니다.

```z
box add(box a, box b) {
    return a + b
}

void greet() {
    print("Hello\n")
}
```

타입을 적은 매개변수는 함수 호출 시 인수 타입도 검사합니다. 기존의 `add(a, b)` 선언도 계속 사용할 수 있습니다.

변수는 선언된 함수와 블록 범위에서 사용할 수 있습니다. 같은 이름을 다시 선언하거나 바깥 범위의 변수 이름을 가리는 선언은 허용되지 않습니다. `for` 반복 변수는 반복문 안에서만 보이며 읽기 전용입니다.

`complex(real)` 또는 `complex(real, imaginary)`는 복소수를 만드는 내장 함수입니다. 인수는 `box` 또는 `float` 값이어야 합니다.

`vector<T>`와 `array<T, N>`은 같은 타입의 값을 담습니다. 대괄호로 초기화하고, 인덱스는 0부터 시작합니다. 벡터는 가변 길이이고 배열은 선언한 크기로 고정됩니다. 배열은 초기화할 때 원소 수가 N과 같아야 합니다. 두 타입 모두 중첩해 행렬처럼 사용할 수 있고 `.size()`는 해당 행 또는 컬렉션의 원소 개수를 반환합니다. 벡터는 `.push(value)`로 끝에 값을 추가하고 `.pop()`으로 마지막 값을 제거해 돌려받을 수 있습니다. 빈 벡터에서 `pop()`을 호출하면 실행 오류가 납니다. 고정 배열에는 `push()`와 `pop()`을 사용할 수 없습니다.

```z
main() {
    vector<vector<box>> matrix = [[1, 2, 3], [4, 5, 6]]
    matrix[1][2] = 9

    for row in +(matrix.size()) {
        for col in +(matrix[row].size()) {
            print(matrix[row][col], " ")
        }
        print("\n")
    }
}
```

고정 크기 배열은 다음처럼 선언합니다. 아래는 2행 3열이며, `matrix.size()`는 2, `matrix[0].size()`는 3을 반환합니다.

```z
array<array<box, 3>, 2> matrix = [[1, 2, 3], [4, 5, 6]]
matrix[1][2] = 9
print(matrix[1][2], "\n")
```

범위를 벗어난 인덱스는 실행 중 오류가 됩니다.

## 4. 식과 연산자

지원하는 리터럴은 정수, 소수, 문자열, `true`, `false`입니다. 숫자는 `12`, `3.5`처럼 씁니다. 소수점 뒤에는 숫자가 필요하고, 지수 표기법은 지원하지 않습니다.

연산자 우선순위는 높은 순서부터 다음과 같습니다.

| 순위 | 연산자 | 결합 방향 |
| --- | --- | --- |
| 1 | 함수 호출 `f(...)`, 괄호 `(식)` | 왼쪽부터 |
| 2 | 단항 `+`, `-`, `!` | 오른쪽부터 |
| 3 | `*`, `/`, `%` | 왼쪽부터 |
| 4 | `+`, `-` | 왼쪽부터 |
| 5 | `<`, `<=`, `>`, `>=` | 왼쪽부터 |
| 6 | `==`, `!=` | 왼쪽부터 |
| 7 | `&&` | 왼쪽부터 |
| 8 | `or` | 왼쪽부터 |

예를 들어 `2 + 3 * 4`는 `2 + (3 * 4)`로 계산합니다.

주요 타입 규칙:

- `box`끼리 `+`, `-`, `*`, `%`를 하면 `box`가 됩니다. `%`는 정수끼리만 사용할 수 있고 0으로 나머지를 구할 수 없습니다. `float`이 포함된 산술 결과와 `box`/`float` 나눗셈 결과는 `float`입니다.
- `conda + conda`는 문자열을 이어 붙입니다.
- `complex`끼리 `+`, `-`, `*`, `/`를 할 수 있습니다.
- `==`, `!=`는 같은 타입 값끼리 비교합니다.
- `&&`는 `bool` 값 두 개를 연결하며, 왼쪽이 `false`이면 오른쪽 식을 평가하지 않습니다.
- `or`는 `bool` 값 두 개를 연결하며, 왼쪽이 `true`이면 오른쪽 식을 평가하지 않습니다. `||` 대신 `or`를 씁니다.
- 단항 `!`는 `bool` 값을 반전합니다. 예: `!ready`. `not` 키워드는 쓰지 않습니다.
- `<`, `<=`, `>`, `>=`는 `box` 또는 `float` 값끼리 비교할 수 있습니다.
- 단항 `+`, `-`는 `box`와 `float`에, 단항 `!`는 `bool`에 사용할 수 있습니다.
- `if`와 `while` 조건은 `bool`이어야 합니다.
- `box(value)`와 `float(value)`는 숫자 또는 숫자 문자열을 변환합니다. 실수를 `box`로 바꾸면 소수 부분을 0 방향으로 버립니다.
- `input()`은 한 줄을 문자열(`conda`)로 읽습니다. `input("이름: ")`처럼 안내 문자열을 줄 수도 있습니다.
- `type(value)`는 값의 타입 이름을 문자열로 돌려줍니다. 예를 들어 `print(type(3))`은 `box`를 출력합니다.

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

for i in range(1, 10, 2) {
    print(i, " ")
}

box age = box(input("나이: "))
print(type(age), "\n")

for i in +(10) {
    if (i == 3) {
        brk
    }
}

for i in range(5) {
    if (i == 2) {
        continue
    }
    print(i, " ")
}
```

`for i in +(N)`은 0부터 N-1까지 반복하는 기존 표현입니다. `range(end)`, `range(start, end)`, `range(start, end, step)`도 사용할 수 있으며 끝값은 포함하지 않습니다. 간격은 생략하면 1입니다. 음수 간격도 가능하고, 간격 0은 실행 중 오류입니다. 범위의 값은 `box` 정수여야 합니다.

## 6. 현재 지원하지 않는 문법과 주의점

현재 구현에는 클래스와 사용자 정의 타입 선언이 없습니다. 복합 대입(`+=`)과 대입식을 식 안에 넣는 기능도 없습니다.

반환 타입 추론은 단순합니다. 특히 다른 함수의 반환 타입을 호출 순서와 관계없이 완전하게 추론하는 기능은 구현되어 있지 않으므로, 반환 타입이 중요한 함수는 반환 타입을 명시하거나 사용을 단순하게 유지하는 편이 안전합니다. 매개변수 타입은 전부 생략하거나 전부 명시해야 하며, 한 함수 안에서 두 방식을 섞을 수 없습니다.

실행은 다음 순서로 진행됩니다: 렉싱 → 파싱 및 AST 생성 → 의미 분석 → C++17 생성 → 네이티브 컴파일 → 실행. 실행 중 0으로 나누기 같은 오류는 `runtime error`로 보고됩니다.
