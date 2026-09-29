#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace zlang { struct ProcessResult{int exitCode;std::string command;}; ProcessResult runProcess(const std::filesystem::path&,const std::vector<std::string>&); std::filesystem::path findCompiler(const std::optional<std::filesystem::path>&,std::vector<std::string>&); }
