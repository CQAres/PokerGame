#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "common/json.h"
#include "common/result.h"

namespace poker::common {

// Read-only JSON configuration view.
//
// Paths use dots for object keys and array indexes:
//   table.starting_stack
//   seats.0.name
class Config {
public:
    static Result<Config> FromFile(const std::filesystem::path& path);

    static Result<Config> FromJson(Json root);

    const Json& root() const noexcept { return root_; }

    [[nodiscard]] bool Contains(std::string_view path) const;

    Result<Json> Get(std::string_view path) const;

    Result<int64_t> GetInt(std::string_view path) const;

    Result<double> GetDouble(std::string_view path) const;

    Result<bool> GetBool(std::string_view path) const;

    Result<std::string> GetString(std::string_view path) const;

    Result<int64_t> GetIntOr(std::string_view path, int64_t fallback) const;

    Result<bool> GetBoolOr(std::string_view path, bool fallback) const;

    Result<std::string> GetStringOr(std::string_view path,
                                    std::string fallback) const;

private:
    explicit Config(Json root);

    static const Json* Find(const Json& root, std::string_view path,
                            std::string& error);

    Json root_;
};

} // namespace poker::common
