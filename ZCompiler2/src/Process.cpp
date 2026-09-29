#include "zlang/Process.h"

#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace zlang {

    namespace {

        std::string quoteArgument(const std::string& argument)
        {
            std::string result = "\"";
            std::size_t backslashCount = 0;

            for (const char character : argument) {
                if (character == '\\') {
                    ++backslashCount;
                    continue;
                }

                if (character == '"') {
                    result.append(
                        backslashCount * 2 + 1,
                        '\\'
                    );

                    result += '"';
                    backslashCount = 0;
                    continue;
                }

                result.append(
                    backslashCount,
                    '\\'
                );

                backslashCount = 0;
                result += character;
            }

            result.append(
                backslashCount * 2,
                '\\'
            );

            result += '"';
            return result;
        }

    } // namespace

    ProcessResult runProcess(
        const std::filesystem::path& executable,
        const std::vector<std::string>& arguments
    )
    {
        std::string command = quoteArgument(
            executable.string()
        );

        for (const auto& argument : arguments) {
            command += ' ';
            command += quoteArgument(argument);
        }

#ifdef _WIN32

        const std::wstring wideExecutable =
            executable.wstring();

        const std::wstring wideCommand(
            command.begin(),
            command.end()
        );

        std::vector<wchar_t> commandBuffer(
            wideCommand.begin(),
            wideCommand.end()
        );

        commandBuffer.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo{};

        const BOOL processCreated = CreateProcessW(
            wideExecutable.c_str(),
            commandBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo
        );

        if (!processCreated) {
            return {
                static_cast<int>(GetLastError()),
                command
            };
        }

        WaitForSingleObject(
            processInfo.hProcess,
            INFINITE
        );

        DWORD exitCode = 0;

        GetExitCodeProcess(
            processInfo.hProcess,
            &exitCode
        );

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        return {
            static_cast<int>(exitCode),
            command
        };

#else

        const int processResult = std::system(
            command.c_str()
        );

        const int exitCode =
            WIFEXITED(processResult)
            ? WEXITSTATUS(processResult)
            : processResult;

        return {
            exitCode,
            command
        };

#endif
    }

    std::filesystem::path findCompiler(
        const std::optional<std::filesystem::path>& specifiedCompiler,
        std::vector<std::string>& triedCompilers
    )
    {
        // --cxx로 지정된 컴파일러는 검사하지 않고 바로 사용합니다.
        if (specifiedCompiler.has_value()) {
            triedCompilers.push_back(
                specifiedCompiler->string()
            );

            return specifiedCompiler.value();
        }

        std::vector<std::filesystem::path> candidates;

        if (const char* configuredCompiler =
            std::getenv("ZLANG_CXX")) {
            candidates.emplace_back(
                configuredCompiler
            );
        }

        if (const char* environmentCompiler =
            std::getenv("CXX")) {
            candidates.emplace_back(
                environmentCompiler
            );
        }

#ifdef _WIN32

        candidates.emplace_back("cl.exe");
        candidates.emplace_back("clang++.exe");
        candidates.emplace_back("g++.exe");

#else

        candidates.emplace_back("clang++");
        candidates.emplace_back("g++");
        candidates.emplace_back("c++");

#endif

        for (const auto& compilerPath : candidates) {
            triedCompilers.push_back(
                compilerPath.string()
            );

            std::vector<std::string> versionArguments;

            if (compilerPath.filename() == "cl.exe") {
                versionArguments = {
                    "/nologo",
                    "/Bv"
                };
            }
            else {
                versionArguments = {
                    "--version"
                };
            }

            const ProcessResult result = runProcess(
                compilerPath,
                versionArguments
            );

            if (result.exitCode == 0) {
                return compilerPath;
            }
        }

        return {};
    }

} // namespace zlang