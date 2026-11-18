#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "common/config.h"
#include "common/ids.h"
#include "common/json.h"
#include "common/logging.h"
#include "common/result.h"
#include "common/time.h"

namespace {

using poker::common::Config;
using poker::common::Json;
using poker::common::MakeError;
using poker::common::MakeId;
using poker::common::Result;
using poker::common::Status;

TEST(ResultTest, OkAndError) {
    Result<int> ok(42);
    ASSERT_TRUE(ok.ok());
    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_EQ(ok.value(), 42);

    Result<std::string> error =
        MakeError("test_error", "something went wrong");
    ASSERT_FALSE(error.ok());
    EXPECT_FALSE(static_cast<bool>(error));
    EXPECT_EQ(error.error().code, "test_error");
    EXPECT_EQ(error.error().message, "something went wrong");
    EXPECT_EQ(error.value_or("fallback"), "fallback");
}

TEST(ResultTest, Status) {
    const Status ok = Status::Ok();
    EXPECT_TRUE(ok.ok());

    const Status error = MakeError("cancelled", "user cancelled");
    EXPECT_FALSE(error.ok());
    EXPECT_EQ(error.error().code, "cancelled");
}

TEST(JsonTest, ParseAndDump) {
    auto parsed = poker::common::ParseJson(R"({"name":"Alice","stack":1000})");
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    EXPECT_EQ(parsed.value().at("name").get<std::string>(), "Alice");
    EXPECT_EQ(parsed.value().at("stack").get<int>(), 1000);

    const std::string compact = poker::common::DumpJson(parsed.value());
    EXPECT_EQ(compact.find('\n'), std::string::npos);

    const std::string pretty = poker::common::DumpJson(parsed.value(), true);
    EXPECT_NE(pretty.find('\n'), std::string::npos);
}

TEST(JsonTest, ParseError) {
    auto parsed = poker::common::ParseJson("{not-json}");
    ASSERT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error().code, "json_parse_error");
    EXPECT_FALSE(parsed.error().message.empty());
}

TEST(JsonTest, FileRoundTrip) {
    const auto path = std::filesystem::temp_directory_path() /
                      (MakeId("poker-json-test-") + ".json");
    const Json value = {{"table", "t1"}, {"players", 6}};

    const auto saved = poker::common::SaveJsonFile(path, value, true);
    ASSERT_TRUE(saved.ok()) << saved.error().message;

    auto loaded = poker::common::LoadJsonFile(path);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(loaded.value().at("table").get<std::string>(), "t1");
    EXPECT_EQ(loaded.value().at("players").get<int>(), 6);

    std::filesystem::remove(path);
}

TEST(ConfigTest, NestedValuesAndArrayIndexes) {
    auto parsed = poker::common::ParseJson(R"({
        "table": {
            "max_players": 6,
            "small_blind": 5,
            "big_blind": 10,
            "enabled": true,
            "rake": 0.0
        },
        "seats": [
            {"name": "Alice", "type": "human"},
            {"name": "RuleBot", "type": "rule"}
        ]
    })");
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;

    auto config = Config::FromJson(std::move(parsed.value()));
    ASSERT_TRUE(config.ok()) << config.error().message;

    const auto& value = config.value();
    EXPECT_TRUE(value.Contains("table.max_players"));
    EXPECT_TRUE(value.Contains("seats.1.name"));
    EXPECT_FALSE(value.Contains("table.missing"));

    auto players = value.GetInt("table.max_players");
    ASSERT_TRUE(players.ok()) << players.error().message;
    EXPECT_EQ(players.value(), 6);

    auto bigBlind = value.GetInt("table.big_blind");
    ASSERT_TRUE(bigBlind.ok());
    EXPECT_EQ(bigBlind.value(), 10);

    auto enabled = value.GetBool("table.enabled");
    ASSERT_TRUE(enabled.ok());
    EXPECT_TRUE(enabled.value());

    auto rake = value.GetDouble("table.rake");
    ASSERT_TRUE(rake.ok());
    EXPECT_DOUBLE_EQ(rake.value(), 0.0);

    auto name = value.GetString("seats.1.name");
    ASSERT_TRUE(name.ok());
    EXPECT_EQ(name.value(), "RuleBot");
}

TEST(ConfigTest, LoadFromFile) {
    const auto path = std::filesystem::temp_directory_path() /
                      (MakeId("poker-config-test-") + ".json");
    const Json value = {{"table", {{"max_players", 6}, {"big_blind", 10}}}};

    const auto saved = poker::common::SaveJsonFile(path, value, true);
    ASSERT_TRUE(saved.ok()) << saved.error().message;

    auto config = Config::FromFile(path);
    ASSERT_TRUE(config.ok()) << config.error().message;

    auto maxPlayers = config.value().GetInt("table.max_players");
    ASSERT_TRUE(maxPlayers.ok());
    EXPECT_EQ(maxPlayers.value(), 6);

    std::filesystem::remove(path);
}

TEST(ConfigTest, MissingAndTypeErrors) {
    auto parsed = poker::common::ParseJson(R"({
        "table": {"max_players": 6, "name": "main"}
    })");
    ASSERT_TRUE(parsed.ok());

    auto config = Config::FromJson(std::move(parsed.value()));
    ASSERT_TRUE(config.ok());
    const auto& value = config.value();

    auto missing = value.GetInt("table.small_blind");
    ASSERT_FALSE(missing.ok());
    EXPECT_EQ(missing.error().code, "config_path");

    auto wrongType = value.GetString("table.max_players");
    ASSERT_FALSE(wrongType.ok());
    EXPECT_EQ(wrongType.error().code, "config_type");

    auto invalidIndex = value.GetInt("table.0");
    ASSERT_FALSE(invalidIndex.ok());
    EXPECT_EQ(invalidIndex.error().code, "config_path");
}

TEST(ConfigTest, Defaults) {
    auto parsed = poker::common::ParseJson(R"({"table":{"max_players":6}})");
    ASSERT_TRUE(parsed.ok());

    auto config = Config::FromJson(std::move(parsed.value()));
    ASSERT_TRUE(config.ok());
    const auto& value = config.value();

    auto missingInt = value.GetIntOr("table.small_blind", 5);
    ASSERT_TRUE(missingInt.ok());
    EXPECT_EQ(missingInt.value(), 5);

    auto missingBool = value.GetBoolOr("table.auto_start", false);
    ASSERT_TRUE(missingBool.ok());
    EXPECT_FALSE(missingBool.value());

    auto missingString = value.GetStringOr("table.name", "default");
    ASSERT_TRUE(missingString.ok());
    EXPECT_EQ(missingString.value(), "default");

    auto wrongType = value.GetIntOr("table.max_players", 2);
    ASSERT_TRUE(wrongType.ok());
    EXPECT_EQ(wrongType.value(), 6);
}

TEST(TimeTest, UnixMillisAndIsoFormat) {
    const auto now = poker::common::SystemClock::now();
    EXPECT_GT(poker::common::UnixMillis(now), 0);

    const std::string text = poker::common::FormatIso8601Utc(now);
    const std::regex pattern(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$)");
    EXPECT_TRUE(std::regex_match(text, pattern)) << text;

    const auto epoch = poker::common::SystemClock::time_point{};
    EXPECT_EQ(poker::common::FormatIso8601Utc(epoch),
              "1970-01-01T00:00:00.000Z");
}

TEST(IdsTest, MonotonicAndUnique) {
    const uint64_t first = poker::common::NextId();
    const uint64_t second = poker::common::NextId();
    EXPECT_LT(first, second);

    std::set<std::string> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.insert(MakeId("req-"));
    }
    EXPECT_EQ(ids.size(), 1000u);
    EXPECT_EQ(ids.begin()->rfind("req-", 0), 0u);
}

TEST(LoggingTest, WritesToFile) {
    const auto path = std::filesystem::temp_directory_path() /
                      (MakeId("poker-log-test-") + ".log");

    poker::common::LogOptions options;
    options.level = "debug";
    options.console = false;
    options.file = path;

    const auto initialized = poker::common::InitLogging(options);
    ASSERT_TRUE(initialized.ok()) << initialized.error().message;

    poker::common::Log()->info("hello {}", 42);
    poker::common::ShutdownLogging();

    ASSERT_TRUE(std::filesystem::exists(path));
    std::string contents;
    {
        std::ifstream input(path);
        contents.assign((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    }
    EXPECT_NE(contents.find("hello 42"), std::string::npos);

    std::filesystem::remove(path);
}

} // namespace
