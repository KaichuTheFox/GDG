cmake_minimum_required(VERSION 3.20)
project(ZCompiler VERSION 0.2.3 LANGUAGES CXX)

include(CTest)
option(ZLANG_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(ZLANG_SOURCES
    src/CodeGen.cpp
    src/Diagnostic.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Process.cpp
    src/Semantic.cpp
)

add_library(zlang_core STATIC ${ZLANG_SOURCES})
target_include_directories(zlang_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(zlang_core PUBLIC cxx_std_17)

add_executable(zlang src/main.cpp)
target_link_libraries(zlang PRIVATE zlang_core)
target_compile_features(zlang PRIVATE cxx_std_17)

function(zlang_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
        if(ZLANG_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(ZLANG_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

zlang_enable_warnings(zlang_core)
zlang_enable_warnings(zlang)

set_target_properties(zlang PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

if(BUILD_TESTING)
    add_executable(zlang_unit tests/unit.cpp)
    target_link_libraries(zlang_unit PRIVATE zlang_core)
    zlang_enable_warnings(zlang_unit)
    set_target_properties(zlang_unit PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME unit COMMAND zlang_unit)

    if(WIN32)
        add_test(
            NAME windows_compatibility
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_test.ps1"
                    -Zlang "$<TARGET_FILE:zlang>"
                    -Source "${CMAKE_CURRENT_SOURCE_DIR}/examples/compatibility.z"
                    -Expected "${CMAKE_CURRENT_SOURCE_DIR}/examples/compatibility.stdout"
                    -Work "${CMAKE_BINARY_DIR}/integration"
                    -Cxx "${CMAKE_CXX_COMPILER}"
        )
    endif()
endif()
