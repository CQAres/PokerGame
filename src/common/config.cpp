#include "common/config.h"

#include <charconv>
#include <limits>
#include <sstream>

namespace poker::common {
namespace {

Result<Json> MissingPath(std::string_view path, std::string detail) {
    return MakeError("config_path",
                     "configuration path '" + std::string(path) +
                         "': " + std::move(detail));
}

Error TypeMismatch(std::string_view path, std::string_view expected,
                   const Json& value) {
    return MakeError("config_type",
                     "configuration path '" + std::string(path) +
                         "' must be " + std::string(expected) + ", got " +
                         value.type_name());
}

} // namespace

Config::Config(Json root) : root_(std::move(root)) {}

Result<Config> Config::FromFile(const std::filesystem::path& path) {
    auto loaded = LoadJsonFile(path);
    if (!loaded.ok()) {
        return loaded.error();
    }
    return FromJson(std::move(loaded.value()));
}

Result<Config> Config::FromJson(Json root) {
    if (!root.is_object()) {
        return MakeError("config_invalid_root",
                         "configuration root must be a JSON object");
    }
    return Config(std::move(root));
}

const Json* Config::Find(const Json& root, std::string_view path,
                         std::string& error) {
    if (path.empty()) {
        return &root;
    }

    const Json* current = &root;
    size_t start = 0;
    while (start <= path.size()) {
        const size_t dot = path.find('.', start);
        const size_t length =
            (dot == std::string_view::npos) ? path.size() - start : dot - start;
        const std::string_view segment = path.substr(start, length);
        if (segment.empty()) {
            error = "empty path segment";
            return nullptr;
        }

        if (current->is_object()) {
            const auto found = current->find(std::string(segment));
            if (found == current->end()) {
                error = "missing key '" + std::string(segment) + "'";
                return nullptr;
            }
            current = &*found;
        } else if (current->is_array()) {
            size_t index = 0;
            const char* begin = segment.data();
            const char* end = begin + segment.size();
            const auto result = std::from_chars(begin, end, index);
            if (result.ec != std::errc{} || result.ptr != end) {
                error = "array index '" + std::string(segment) + "' is not an integer";
                return nullptr;
            }
            if (index >= current->size()) {
                std::ostringstream message;
                message << "array index " << index << " out of range (size "
                        << current->size() << ')';
                error = message.str();
                return nullptr;
            }
            current = &(*current)[index];
        } else {
            error = "cannot descend into " + std::string(current->type_name());
            return nullptr;
        }

        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return current;
}

bool Config::Contains(std::string_view path) const {
    std::string error;
    return Find(root_, path, error) != nullptr;
}

Result<Json> Config::Get(std::string_view path) const {
    std::string error;
    const Json* value = Find(root_, path, error);
    if (value == nullptr) {
        return MissingPath(path, std::move(error));
    }
    return *value;
}

Result<int64_t> Config::GetInt(std::string_view path) const {
    auto value = Get(path);
    if (!value.ok()) {
        return value.error();
    }
    if (value.value().is_number_unsigned()) {
        const auto unsignedValue = value.value().get<uint64_t>();
        if (unsignedValue > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return MakeError("config_range",
                             "configuration path '" + std::string(path) +
                                 "' is outside int64 range");
        }
        return static_cast<int64_t>(unsignedValue);
    }
    if (value.value().is_number_integer()) {
        return value.value().get<int64_t>();
    }
    return TypeMismatch(path, "an integer", value.value());
}

Result<double> Config::GetDouble(std::string_view path) const {
    auto value = Get(path);
    if (!value.ok()) {
        return value.error();
    }
    if (!value.value().is_number()) {
        return TypeMismatch(path, "a number", value.value());
    }
    return value.value().get<double>();
}

Result<bool> Config::GetBool(std::string_view path) const {
    auto value = Get(path);
    if (!value.ok()) {
        return value.error();
    }
    if (!value.value().is_boolean()) {
        return TypeMismatch(path, "a boolean", value.value());
    }
    return value.value().get<bool>();
}

Result<std::string> Config::GetString(std::string_view path) const {
    auto value = Get(path);
    if (!value.ok()) {
        return value.error();
    }
    if (!value.value().is_string()) {
        return TypeMismatch(path, "a string", value.value());
    }
    return value.value().get<std::string>();
}

Result<int64_t> Config::GetIntOr(std::string_view path, int64_t fallback) const {
    auto value = GetInt(path);
    if (value.ok()) {
        return value;
    }
    if (value.error().code == "config_path") {
        return fallback;
    }
    return value.error();
}

Result<bool> Config::GetBoolOr(std::string_view path, bool fallback) const {
    auto value = GetBool(path);
    if (value.ok()) {
        return value;
    }
    if (value.error().code == "config_path") {
        return fallback;
    }
    return value.error();
}

Result<std::string> Config::GetStringOr(std::string_view path,
                                        std::string fallback) const {
    auto value = GetString(path);
    if (value.ok()) {
        return value;
    }
    if (value.error().code == "config_path") {
        return fallback;
    }
    return value.error();
}

} // namespace poker::common
