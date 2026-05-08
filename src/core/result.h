#pragma once

#include <string>
#include <utility>

namespace streamrelay::core {

enum class ErrorCode {
    Ok = 0,
    InvalidArgument,
    NotFound,
    AlreadyExists,
    InvalidState,
    ProtocolError,
    DeadlineExceeded,
    ResourceExhausted,
    InternalError,
};

struct Error {
    ErrorCode code{ErrorCode::Ok};
    std::string message;

    bool ok() const noexcept {
        return code == ErrorCode::Ok;
    }
};

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), error_{} {}
    Result(Error error) : value_{}, error_(std::move(error)) {}

    bool ok() const noexcept {
        return error_.ok();
    }

    const T& value() const& {
        return value_;
    }

    T& value() & {
        return value_;
    }

    T&& value() && {
        return std::move(value_);
    }

    const Error& error() const noexcept {
        return error_;
    }

private:
    T value_{};
    Error error_{};
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)) {}

    bool ok() const noexcept {
        return error_.ok();
    }

    const Error& error() const noexcept {
        return error_;
    }

private:
    Error error_{};
};

inline Result<void> success() {
    return Result<void>{};
}

inline Error make_error(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

} // namespace streamrelay::core
