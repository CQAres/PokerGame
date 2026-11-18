#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace poker::common {

struct Error {
    std::string code;
    std::string message;
};

inline Error MakeError(std::string code, std::string message) {
    return Error{std::move(code), std::move(message)};
}

// Status is the no-value equivalent of Result<T>.
class Status {
public:
    Status() noexcept = default;

    Status(Error error) : error_(std::move(error)), ok_(false) {}

    static Status Ok() noexcept { return Status{}; }

    [[nodiscard]] bool ok() const noexcept { return ok_; }

    explicit operator bool() const noexcept { return ok_; }

    const Error& error() const {
        if (ok_) {
            throw std::logic_error("Status::error() called on a successful status");
        }
        return error_;
    }

private:
    Error error_{};
    bool ok_ = true;
};

template <typename T>
class Result {
    static_assert(!std::is_void_v<T>, "Use Status for operations without a value");
    static_assert(!std::is_same_v<T, Error>, "Use Status for error-only results");

public:
    Result(T value) : storage_(std::in_place_index<0>, std::move(value)) {}

    Result(Error error) : storage_(std::in_place_index<1>, std::move(error)) {}

    [[nodiscard]] bool ok() const noexcept {
        return storage_.index() == 0;
    }

    explicit operator bool() const noexcept { return ok(); }

    const T& value() const& { return GetValue(*this); }

    T& value() & { return GetValue(*this); }

    T&& value() && {
        return std::move(GetValue(*this));
    }

    const Error& error() const& {
        if (ok()) {
            throw std::logic_error("Result::error() called on a successful result");
        }
        return std::get<1>(storage_);
    }

    T value_or(T fallback) const& {
        return ok() ? std::get<0>(storage_) : std::move(fallback);
    }

private:
    template <typename Self>
    static auto& GetValue(Self& self) {
        if (!self.ok()) {
            throw std::logic_error("Result::value() called on an error result");
        }
        return std::get<0>(self.storage_);
    }

    std::variant<T, Error> storage_;
};

} // namespace poker::common
