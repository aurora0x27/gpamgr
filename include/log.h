#pragma once

#include "args.h"

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <source_location>

template <>
bool utils::opt<spdlog::level::level_enum>::parse_impl(std::string_view arg);

template <>
void utils::opt<spdlog::level::level_enum>::print() const;

template <>
void utils::opt<spdlog::level::level_enum>::print(std::ostream &os) const;

template <>
bool utils::opt<spdlog::color_mode>::parse_impl(std::string_view arg);

template <>
void utils::opt<spdlog::color_mode>::print() const;

template <>
void utils::opt<spdlog::color_mode>::print(std::ostream &os) const;

namespace {
template <typename... Args>
struct logging_rformat {
    template <std::convertible_to<std::string_view> StrLike>
    consteval logging_rformat(const StrLike &str,
                              std::source_location location = std::source_location::current()) :
        str(str), location(location) {}

    std::format_string<Args...> str;
    std::source_location location;
};

template <typename... Args>
using logging_format = logging_rformat<std::type_identity_t<Args>...>;

template <typename... Args>
void log(spdlog::level::level_enum level,
         std::source_location location,
         std::format_string<Args...> fmt,
         Args &&...args) {
    spdlog::source_loc loc{
        location.file_name(),
        static_cast<int>(location.line()),
        location.function_name(),
    };
    using spdlog_fmt = spdlog::format_string_t<Args...>;
    if constexpr (std::same_as<spdlog_fmt, std::string_view>) {
        spdlog::log(loc, level, fmt.get(), std::forward<Args>(args)...);
    } else {
        spdlog::log(loc, level, fmt, std::forward<Args>(args)...);
    }
}
}  // namespace

namespace logging {
void init_log(const std::string_view name,
              const spdlog::level::level_enum level,
              spdlog::color_mode color,
              std::optional<std::string_view> log_file);

template <typename... Args>
void trace(logging_format<Args...> fmt, Args &&...args) {
    log(spdlog::level::trace, fmt.location, fmt.str, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(logging_format<Args...> fmt, Args &&...args) {
    log(spdlog::level::debug, fmt.location, fmt.str, std::forward<Args>(args)...);
}

template <typename... Args>
void info(logging_format<Args...> fmt, Args &&...args) {
    log(spdlog::level::info, fmt.location, fmt.str, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(logging_format<Args...> fmt, Args &&...args) {
    log(spdlog::level::warn, fmt.location, fmt.str, std::forward<Args>(args)...);
}

template <typename... Args>
void error(logging_format<Args...> fmt, Args &&...args) {
    log(spdlog::level::err, fmt.location, fmt.str, std::forward<Args>(args)...);
}

template <typename... Args>
void critical [[noreturn]] (logging_format<Args...> fmt, Args &&...args) {
    log(spdlog::level::critical, fmt.location, fmt.str, std::forward<Args>(args)...);
    spdlog::shutdown();
    std::abort();
}
}  // namespace logging
