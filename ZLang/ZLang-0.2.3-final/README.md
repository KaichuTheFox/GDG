# Z 언어 컴파일러 0.2.3

Z는 C++17 코드를 생성한 다음 MSVC, Clang 또는 MinGW로 네이티브 실행 파일을 만드는 작은 정적 검사 언어입니다. 현재 구현된 문법과 제한 사항은 [문법 안내서](docs/grammar.md)에 정리되어 있습니다.

## Windows에서 빌드하기

Visual Studio 2022 이상에 **C++를 사용한 데스크톱 개발**과 **Windows용 C++ CMake 도구**를 설치한 뒤 프로젝트 폴더에서 실행하세요.

이 소스 패키지에는 미리 빌드한 실행 파일이 포함되어 있지 않습니다. 아래 명령으로 컴파일러를 빌드하세요.

```powershell
.\BUILD_WINDOWS.bat
```

컴파일러는 `build\windows-x64\bin\zlang.exe`에 생성됩니다. Z 파일 실행:

```powershell
.\build\windows-x64\bin\zlang.exe run main.z
```

단축 스크립트도 사용할 수 있습니다.

```powershell
.\ZBUILD.bat main.z
.\ZRUN.bat main.z
```

직접 사용하는 예:

```powershell
zlang.exe main.z
zlang.exe build main.z -o hello.exe
zlang.exe run main.z
zlang.exe main.z --emit-cpp=main.generated.cpp
```

컴파일러는 설치된 x64 MSVC를 자동으로 찾고 필요한 빌드 환경을 초기화합니다. 다른 C++ 컴파일러를 지정하려면 `--cxx <경로>`를 사용하세요.

## 컴파일 과정

1. `src/Lexer.cpp`가 소스 문자를 토큰으로 나눕니다.
2. `src/Parser.cpp`가 토큰으로 문법 구조를 만들고 `include/zlang/AST.h`에 정의된 AST를 구성합니다.
3. `src/Semantic.cpp`가 변수 선언, 범위, 연산, 함수 호출, 조건식의 타입을 검사합니다.
4. `src/CodeGen.cpp`가 C++17 코드를 만들고, 컴파일러 드라이버가 네이티브 실행 파일을 생성합니다.

Windows용 생성 코드에서는 `<windows.h>`보다 먼저 `NOMINMAX`를 정의해 Windows의 `min`/`max` 매크로가 C++ 표준 함수 호출을 깨뜨리지 않게 했습니다.

## 폴더 구성

- `src/`, `include/`: 컴파일러 구현
- `docs/grammar.md`: 문법, 타입, 연산 우선순위, 제한 사항
- `examples/`: Z 예제
- `tests/`: 단위 테스트와 Windows 통합 검사
- `BUILD_WINDOWS.bat`: 컴파일러 빌드
- `ZBUILD.bat`, `ZRUN.bat`: Z 파일 빌드 또는 빌드 후 실행

이 최종 소스 패키지에는 빌드 캐시, IDE 설정, 이전 버전 사본, 생성된 임시 실행 파일을 넣지 않았습니다.
