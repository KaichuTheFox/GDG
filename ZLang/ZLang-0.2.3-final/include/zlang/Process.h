#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace zlang {

struct ProcessResult {
    int exitCode = -1;
    std::string command;
    std::string errorMessage;
};

ProcessResult runProcess(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments);

std::filesystem::path findCompiler(
    const std::optional<std::filesystem::path>& specifiedCompiler,
    std::vector<std::string>& triedCompilers);

} // namespace zlang
