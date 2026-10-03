#include "zlang/Process.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace zlang {
namespace {

#ifdef _WIN32
std::wstring quoteWindowsArgument(const std::wstring& argument)
{
    if (argument.empty()) {
        return L"\"\"";
    }
    if (argument.find_first_of(L" \t\"") == std::wstring::npos) {
        return argument;
    }

    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(character);
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8,
                                         MB_ERR_INVALID_CHARS,
                                         text.data(),
                                         static_cast<int>(text.size()),
                                         nullptr,
                                         0);
    if (size <= 0) {
        return std::wstring(text.begin(), text.end());
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        result.data(),
                        size);
    return result;
}

std::optional<std::filesystem::path> resolveExecutable(const std::filesystem::path& executable)
{
    if (executable.has_parent_path()) {
        std::error_code ec;
        return std::filesystem::is_regular_file(executable, ec) ? std::optional(executable)
                                                                  : std::nullopt;
    }

    std::vector<wchar_t> buffer(32768);
    const DWORD length = SearchPathW(nullptr,
                                     executable.c_str(),
                                     nullptr,
                                     static_cast<DWORD>(buffer.size()),
                                     buffer.data(),
                                     nullptr);
    if (length == 0 || length >= buffer.size()) {
        return std::nullopt;
    }
    return std::filesystem::path(std::wstring(buffer.data(), length));
}

std::optional<std::filesystem::path> findVcvars64(const std::filesystem::path& compiler)
{
    std::error_code ec;
    auto current = compiler.parent_path();
    for (int depth = 0; depth < 10 && !current.empty(); ++depth) {
        const auto candidate = current / "Auxiliary" / "Build" / "vcvars64.bat";
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
        current = current.parent_path();
    }
    return std::nullopt;
}

std::filesystem::path commandShell()
{
    if (const char* comSpec = std::getenv("ComSpec")) {
        const std::filesystem::path candidate(comSpec);
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return std::filesystem::path("C:\\Windows\\System32\\cmd.exe");
}

void addMsvcCandidatesFromVisualStudioRoot(
    const std::filesystem::path& visualStudioRoot,
    std::vector<std::filesystem::path>& candidates)
{
    std::error_code ec;
    const auto vcTools = visualStudioRoot / "VC" / "Tools" / "MSVC";
    if (!std::filesystem::is_directory(vcTools, ec)) {
        return;
    }

    std::vector<std::filesystem::path> versions;
    for (const auto& version : std::filesystem::directory_iterator(vcTools, ec)) {
        if (ec) {
            break;
        }
        if (version.is_directory(ec)) {
            versions.push_back(version.path());
        }
    }
    std::sort(versions.begin(), versions.end(), [](const auto& left, const auto& right) {
        return left.filename().string() > right.filename().string();
    });

    for (const auto& version : versions) {
        const auto compiler = version / "bin" / "Hostx64" / "x64" / "cl.exe";
        if (std::filesystem::is_regular_file(compiler, ec)) {
            candidates.push_back(compiler);
        }
    }
}

std::vector<std::filesystem::path> queryVsWhereInstallations()
{
    std::vector<std::filesystem::path> installations;

    std::vector<std::filesystem::path> vswhereCandidates;
    if (const char* programFilesX86 = std::getenv("ProgramFiles(x86)")) {
        vswhereCandidates.emplace_back(std::filesystem::path(programFilesX86) /
                                       "Microsoft Visual Studio" / "Installer" / "vswhere.exe");
    }
    if (const char* programFiles = std::getenv("ProgramFiles")) {
        vswhereCandidates.emplace_back(std::filesystem::path(programFiles) /
                                       "Microsoft Visual Studio" / "Installer" / "vswhere.exe");
    }
    vswhereCandidates.emplace_back(
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe");
    vswhereCandidates.emplace_back(
        "C:\\Program Files\\Microsoft Visual Studio\\Installer\\vswhere.exe");

    std::filesystem::path vswhere;
    for (const auto& candidate : vswhereCandidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            vswhere = candidate;
            break;
        }
    }
    if (vswhere.empty()) {
        return installations;
    }

    const std::string command = quoteWindowsArgument(vswhere.wstring()).empty()
        ? std::string()
        : "\"" + vswhere.string() + "\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";
    if (command.empty()) {
        return installations;
    }

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        return installations;
    }

    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (!line.empty()) {
            installations.emplace_back(line);
        }
    }
    _pclose(pipe);
    return installations;
}

void addMsvcCandidates(std::vector<std::filesystem::path>& candidates)
{
    std::vector<std::filesystem::path> visualStudioRoots;

    if (const char* installDir = std::getenv("VSINSTALLDIR")) {
        visualStudioRoots.emplace_back(installDir);
    }

    for (const auto& installation : queryVsWhereInstallations()) {
        visualStudioRoots.push_back(installation);
    }

    // Explicit roots make discovery independent of WOW64 environment
    // redirection and of the shell from which zlang.exe was launched.
    visualStudioRoots.emplace_back(
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Professional");
    visualStudioRoots.emplace_back(
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Community");
    visualStudioRoots.emplace_back(
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Enterprise");
    visualStudioRoots.emplace_back(
        "C:\\Program Files\\Microsoft Visual Studio\\17\\Professional");
    visualStudioRoots.emplace_back(
        "C:\\Program Files\\Microsoft Visual Studio\\17\\Community");
    visualStudioRoots.emplace_back(
        "C:\\Program Files\\Microsoft Visual Studio\\17\\Enterprise");

    if (const char* programFiles = std::getenv("ProgramFiles")) {
        visualStudioRoots.emplace_back(std::filesystem::path(programFiles) /
                                       "Microsoft Visual Studio" / "18" / "Professional");
        visualStudioRoots.emplace_back(std::filesystem::path(programFiles) /
                                       "Microsoft Visual Studio" / "17" / "Professional");
    }
    if (const char* programFilesX86 = std::getenv("ProgramFiles(x86)")) {
        visualStudioRoots.emplace_back(std::filesystem::path(programFilesX86) /
                                       "Microsoft Visual Studio" / "18" / "Professional");
        visualStudioRoots.emplace_back(std::filesystem::path(programFilesX86) /
                                       "Microsoft Visual Studio" / "17" / "Professional");
    }

    std::vector<std::filesystem::path> deduplicatedRoots;
    for (const auto& root : visualStudioRoots) {
        std::error_code ec;
        const auto normalized = std::filesystem::weakly_canonical(root, ec);
        if (ec) {
            continue;
        }
        if (std::find(deduplicatedRoots.begin(), deduplicatedRoots.end(), normalized) ==
            deduplicatedRoots.end()) {
            deduplicatedRoots.push_back(normalized);
        }
    }

    for (const auto& root : deduplicatedRoots) {
        addMsvcCandidatesFromVisualStudioRoot(root, candidates);
    }

    // Do not recursively scan the whole Visual Studio installation here.
    // A recursive scan can touch localized/non-ASCII file names and make
    // std::filesystem::path::string() throw ERROR_NO_UNICODE_TRANSLATION on
    // Windows. The supported installation roots above already cover custom
    // MSVC versions, while vswhere handles normal Visual Studio installs.
}

#else
std::string quotePosixArgument(const std::string& argument)
{
    std::string result = "'";
    for (const char character : argument) {
        result += character == '\'' ? "'\\''" : std::string(1, character);
    }
    result.push_back('\'');
    return result;
}
#endif

} // namespace

ProcessResult runProcess(const std::filesystem::path& executable,
                         const std::vector<std::string>& arguments)
{
#ifdef _WIN32
    const auto resolved = resolveExecutable(executable);
    if (!resolved) {
        return {2, executable.string(), "executable not found"};
    }

    std::filesystem::path launchExecutable = *resolved;
    std::wstring commandLine;
    const std::string resolvedName = resolved->filename().string();
    const bool isMsvcCompiler = resolvedName == "cl.exe" || resolvedName == "CL.EXE";

    if (isMsvcCompiler) {
        // Visual Studio debugger sessions do not necessarily inherit the
        // Developer Command Prompt environment. Bootstrap x64 MSVC so that
        // INCLUDE, LIB and related variables are available to cl.exe.
        if (const auto vcvars64 = findVcvars64(*resolved)) {
            launchExecutable = commandShell();
            std::wstring inner = L"call ";
            inner += quoteWindowsArgument(vcvars64->wstring());
            inner += L" && ";
            inner += quoteWindowsArgument(resolved->wstring());
            for (const auto& argument : arguments) {
                inner.push_back(L' ');
                inner += quoteWindowsArgument(utf8ToWide(argument));
            }

            // lpApplicationName already points at cmd.exe, so the command line
            // must contain only cmd's arguments. Avoid the /s + nested-quote
            // form here: it changes cmd.exe's quote parsing and can turn valid
            // paths such as "C:\\Program Files\\..." into a malformed command.
            // /c is sufficient because `call` starts the batch file and `&&`
            // continues with cl.exe after vcvars64.bat returns successfully.
            commandLine = L"/d /c ";
            commandLine += inner;
        } else {
            commandLine = quoteWindowsArgument(resolved->wstring());
            for (const auto& argument : arguments) {
                commandLine.push_back(L' ');
                commandLine += quoteWindowsArgument(utf8ToWide(argument));
            }
        }
    } else {
        commandLine = quoteWindowsArgument(resolved->wstring());
        for (const auto& argument : arguments) {
            commandLine.push_back(L' ');
            commandLine += quoteWindowsArgument(utf8ToWide(argument));
        }
    }

    std::vector<wchar_t> writable(commandLine.begin(), commandLine.end());
    writable.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(launchExecutable.c_str(),
                                        writable.data(),
                                        nullptr,
                                        nullptr,
                                        FALSE,
                                        0,
                                        nullptr,
                                        nullptr,
                                        &startup,
                                        &process);
    if (!created) {
        return {static_cast<int>(GetLastError()), executable.string(), "CreateProcessW failed"};
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return {static_cast<int>(exitCode), executable.string(), {}};
#else
    std::string command = quotePosixArgument(executable.string());
    for (const auto& argument : arguments) {
        command += ' ' + quotePosixArgument(argument);
    }
    const int result = std::system(command.c_str());
    return {WIFEXITED(result) ? WEXITSTATUS(result) : result, command, {}};
#endif
}

std::filesystem::path findCompiler(
    const std::optional<std::filesystem::path>& specifiedCompiler,
    std::vector<std::string>& triedCompilers)
{
    if (specifiedCompiler) {
        triedCompilers.push_back(specifiedCompiler->string());
#ifdef _WIN32
        const auto resolved = resolveExecutable(*specifiedCompiler);
        return resolved.value_or(std::filesystem::path{});
#else
        return *specifiedCompiler;
#endif
    }

    std::vector<std::filesystem::path> candidates;
    if (const char* configured = std::getenv("ZLANG_CXX")) {
        candidates.emplace_back(configured);
    }
    if (const char* environment = std::getenv("CXX")) {
        candidates.emplace_back(environment);
    }
#ifdef _WIN32
    // Prefer a native x64 MSVC compiler even when Visual Studio itself was
    // launched from a normal shell instead of a Developer Command Prompt.
    addMsvcCandidates(candidates);
    candidates.emplace_back("cl.exe");
    candidates.emplace_back("clang++.exe");
    candidates.emplace_back("g++.exe");
#else
    candidates.emplace_back("clang++");
    candidates.emplace_back("g++");
    candidates.emplace_back("c++");
#endif

    for (const auto& candidate : candidates) {
        triedCompilers.push_back(candidate.string());
#ifdef _WIN32
        if (const auto resolved = resolveExecutable(candidate)) {
            return *resolved;
        }
#else
        if (runProcess(candidate, {"--version"}).exitCode == 0) {
            return candidate;
        }
#endif
    }
    return {};
}

} // namespace zlang
