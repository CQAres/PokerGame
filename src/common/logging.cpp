#include "common/logging.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string_view>
#include <system_error>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace poker::common {
namespace {

std::mutex g_loggerMutex;
std::shared_ptr<spdlog::logger> g_logger;

std::optional<spdlog::level::level_enum> ParseLevel(std::string_view level) {
    std::string normalized(level);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });

    if (normalized == "trace") {
        return spdlog::level::trace;
    }
    if (normalized == "debug") {
        return spdlog::level::debug;
    }
    if (normalized == "info") {
        return spdlog::level::info;
    }
    if (normalized == "warn" || normalized == "warning") {
        return spdlog::level::warn;
    }
    if (normalized == "error" || normalized == "err") {
        return spdlog::level::err;
    }
    if (normalized == "critical" || normalized == "fatal") {
        return spdlog::level::critical;
    }
    if (normalized == "off") {
        return spdlog::level::off;
    }
    return std::nullopt;
}

} // namespace

Status InitLogging(const LogOptions& options) {
    const auto level = ParseLevel(options.level);
    if (!level.has_value()) {
        return MakeError("log_level",
                         "unknown log level: " + options.level);
    }
    if (!options.console && !options.file.has_value()) {
        return MakeError("log_no_sink",
                         "at least one of console/file must be enabled");
    }

    std::vector<spdlog::sink_ptr> sinks;
    if (options.console) {
        auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
        sink->set_pattern(options.pattern);
        sinks.push_back(std::move(sink));
    }

    if (options.file.has_value()) {
        const auto& file = *options.file;
        const auto parent = file.parent_path();
        if (!parent.empty()) {
            std::error_code error;
            std::filesystem::create_directories(parent, error);
            if (error) {
                return MakeError("log_io_error",
                                 "cannot create log directory " +
                                     parent.string() + ": " + error.message());
            }
        }

        try {
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                file.string(), false);
            sink->set_pattern(options.pattern);
            sinks.push_back(std::move(sink));
        } catch (const spdlog::spdlog_ex& error) {
            return MakeError("log_io_error", error.what());
        }
    }

    auto logger =
        std::make_shared<spdlog::logger>("poker", sinks.begin(), sinks.end());
    logger->set_level(*level);
    logger->set_pattern(options.pattern);
    logger->flush_on(options.flush_on_info ? spdlog::level::info
                                           : spdlog::level::warn);

    std::lock_guard<std::mutex> lock(g_loggerMutex);
    if (g_logger) {
        g_logger->flush();
    }
    g_logger = std::move(logger);
    return Status::Ok();
}

void ShutdownLogging() {
    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard<std::mutex> lock(g_loggerMutex);
        logger = std::move(g_logger);
        g_logger.reset();
    }
    if (logger) {
        logger->flush();
    }
}

std::shared_ptr<spdlog::logger> Log() {
    std::lock_guard<std::mutex> lock(g_loggerMutex);
    if (!g_logger) {
        g_logger = spdlog::default_logger();
    }
    return g_logger;
}

} // namespace poker::common
