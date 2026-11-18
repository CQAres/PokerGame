#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/result.h"

namespace poker::common {

using Json = nlohmann::json;

Result<Json> ParseJson(std::string_view text);

std::string DumpJson(const Json& value, bool pretty = false);

Result<Json> LoadJsonFile(const std::filesystem::path& path);

Status SaveJsonFile(const std::filesystem::path& path, const Json& value,
                    bool pretty = true);

} // namespace poker::common
