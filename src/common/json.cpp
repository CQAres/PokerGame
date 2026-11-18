#include "common/json.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace poker::common {

Result<Json> ParseJson(std::string_view text) {
    try {
        return Json::parse(text.begin(), text.end());
    } catch (const Json::parse_error& error) {
        return MakeError("json_parse_error", error.what());
    } catch (const std::exception& error) {
        return MakeError("json_parse_error", error.what());
    }
}

std::string DumpJson(const Json& value, bool pretty) {
    return value.dump(pretty ? 2 : -1, ' ', false,
                      Json::error_handler_t::replace);
}

Result<Json> LoadJsonFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return MakeError("json_io_error",
                         "cannot open JSON file: " + path.string());
    }

    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    if (!input.good() && !input.eof()) {
        return MakeError("json_io_error",
                         "failed to read JSON file: " + path.string());
    }

    auto parsed = ParseJson(text);
    if (!parsed.ok()) {
        return MakeError(parsed.error().code,
                         path.string() + ": " + parsed.error().message);
    }
    return parsed.value();
}

Status SaveJsonFile(const std::filesystem::path& path, const Json& value,
                    bool pretty) {
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            return MakeError("json_io_error",
                             "cannot create directory " + parent.string() + ": " +
                                 error.message());
        }
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return MakeError("json_io_error",
                         "cannot open JSON file for writing: " + path.string());
    }

    output << DumpJson(value, pretty);
    if (pretty) {
        output << '\n';
    }
    if (!output) {
        return MakeError("json_io_error",
                         "failed to write JSON file: " + path.string());
    }
    return Status::Ok();
}

} // namespace poker::common
