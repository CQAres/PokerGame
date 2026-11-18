#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <spdlog/spdlog.h>

#include "common/result.h"

namespace poker::common {

struct LogOptions {
    std::string level = "info";
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v";
    bool console = true;
    std::optional<std::filesystem::path> file;
    bool flush_on_info = true;
};

Status InitLogging(const LogOptions& options);

void ShutdownLogging();

// Returns the configured logger, or spdlog's default logger before InitLogging.
std::shared_ptr<spdlog::logger> Log();

} // namespace poker::common
